// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Rockchip Electronics Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_pci.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/reset.h>
#include <linux/resource.h>
#include <linux/signal.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <uapi/linux/rk-pcie-dma.h>

#include "rockchip-pcie-dma.h"

/* dma transfer */
/*
 * Write buffer format
 * 0	     4               8	       0xc	0x10	SZ_1M
 * ------------------------------------------------------
 * |0x12345678|local idx(0-7)|data size|reserve	|data	|
 * ------------------------------------------------------
 *
 * Byte 3-0: Receiver check if a valid data package arrived
 * Byte 7-4: As a index for data rcv ack buffer
 * Byte 11-8: Actual data size
 *
 * Data rcv ack buffer format
 * 0		4B
 * --------------
 * |0xdeadbeef	|
 * --------------
 *
 * Data free ack buffer format
 * 0		4B
 * --------------
 * |0xcafebabe	|
 * --------------
 *
 *	RC		EP
 * -	---------	---------
 * |	|  1MB	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * |	|	|	|	|
 * 8MB	|wr buf	|  ->	|rd buf	|
 * |	|	|	|	|
 * |	|	|	|	|
 * |	|	|	|	|
 * -	---------	---------
 * |	|  1MB	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * |	|	|	|	|
 * 8MB	|rd buf	|  <-	|wr buf	|
 * |	|	|	|	|
 * |	|	|	|	|
 * |	|	|	|	|
 * -	---------	---------
 * |	|  4B	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * 32B	|	|	|	|
 * |	|scan	|  <-	|data	|
 * |	|	|	|rcv	|
 * |	|	|	|ack	|
 * |	|	|	|send	|
 * -	---------	---------
 * |	|  4B	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * 32B	|data	|  ->	|scan	|
 * |	|rcv	|	|	|
 * |	|ack	|	|	|
 * |	|send	|	|	|
 * |	|	|	|	|
 * -	---------	---------
 * |	|  4B	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * 32B	|	|	|	|
 * |	|scan	|  <-	|data	|
 * |	|	|	|free	|
 * |	|	|	|ack	|
 * |	|	|	|send	|
 * -	---------	---------
 * |	|4B	|	|	|
 * |	|------	|	|	|
 * |	|	|	|	|
 * 32B	|data	|  ->	|scan	|
 * |	|free	|	|	|
 * |	|ack	|	|	|
 * |	|send	|	|	|
 * |	|	|	|	|
 * -	---------	---------
 */

#define NODE_SIZE		(sizeof(unsigned int))
#define PCIE_DMA_ACK_BLOCK_SIZE		(NODE_SIZE * 8)

#define PCIE_DMA_BUF_SIZE	SZ_1M
#define PCIE_DMA_BUF_CNT	8
#define PCIE_DMA_RD_BUF_SIZE	(PCIE_DMA_BUF_SIZE * PCIE_DMA_BUF_CNT)
#define PCIE_DMA_WR_BUF_SIZE	(PCIE_DMA_BUF_SIZE * PCIE_DMA_BUF_CNT)
#define PCIE_DMA_ACK_BASE	(PCIE_DMA_RD_BUF_SIZE + PCIE_DMA_WR_BUF_SIZE)

#define PCIE_DMA_SET_DATA_CHECK_POS	(SZ_1M - 0x4)
#define PCIE_DMA_SET_LOCAL_IDX_POS	(SZ_1M - 0x8)
#define PCIE_DMA_SET_BUF_SIZE_POS	(SZ_1M - 0xc)

#define PCIE_DMA_DATA_CHECK		0x12345678
#define PCIE_DMA_DATA_ACK_CHECK		0xdeadbeef
#define PCIE_DMA_DATA_FREE_ACK_CHECK	0xcafebabe

#define PCIE_DMA_PARAM_SIZE		64
#define PCIE_DMA_CHN0			0x0

struct pcie_misc_dev {
	struct miscdevice dev;
	struct dma_trx_obj *obj;
};

static inline bool is_rc(struct dma_trx_obj *obj)
{
	return (obj->busno == 0);
}

static void rk_pcie_prepare_dma(struct dma_trx_obj *obj,
			unsigned int idx, unsigned int bus_idx,
			unsigned int local_idx, size_t buf_size,
			enum transfer_type type)
{
	struct device *dev = obj->dev;
	phys_addr_t local, bus;
	void *virt;
	u32 *desc;
	unsigned long flags;
	struct dma_table *table = NULL;

	switch (type) {
	case PCIE_DMA_DATA_SND:
		table = obj->table[PCIE_DMA_DATA_SND_TABLE_OFFSET + local_idx];
		table->type = PCIE_DMA_DATA_SND;
		local = obj->mem_start + local_idx * PCIE_DMA_BUF_SIZE;
		virt = obj->mem_base + local_idx * PCIE_DMA_BUF_SIZE;
		bus = obj->mem_start + bus_idx * PCIE_DMA_BUF_SIZE;

		if (!is_rc(obj)) {
			local += PCIE_DMA_RD_BUF_SIZE;
			virt += PCIE_DMA_RD_BUF_SIZE;
			bus += PCIE_DMA_WR_BUF_SIZE;
		}

		dma_sync_single_for_device(dev, local, buf_size, DMA_TO_DEVICE);

		writel(PCIE_DMA_DATA_CHECK, virt + PCIE_DMA_SET_DATA_CHECK_POS);
		writel(local_idx, virt + PCIE_DMA_SET_LOCAL_IDX_POS);
		writel(buf_size, virt + PCIE_DMA_SET_BUF_SIZE_POS);

		buf_size = SZ_1M;
		break;
	case PCIE_DMA_DATA_RCV_ACK:
		table = obj->table[PCIE_DMA_DATA_RCV_ACK_TABLE_OFFSET + idx];
		table->type = PCIE_DMA_DATA_RCV_ACK;
		local = obj->mem_start + PCIE_DMA_ACK_BASE + idx * NODE_SIZE;
		virt = obj->mem_base + PCIE_DMA_ACK_BASE + idx * NODE_SIZE;

		if (is_rc(obj)) {
			local += PCIE_DMA_ACK_BLOCK_SIZE;
			virt += PCIE_DMA_ACK_BLOCK_SIZE;
		}
		bus = local;
		writel(PCIE_DMA_DATA_ACK_CHECK, virt);
		break;
	case PCIE_DMA_DATA_FREE_ACK:
		table = obj->table[PCIE_DMA_DATA_FREE_ACK_TABLE_OFFSET + idx];
		table->type = PCIE_DMA_DATA_FREE_ACK;
		local = obj->mem_start + PCIE_DMA_ACK_BASE + idx * NODE_SIZE;
		virt = obj->mem_base + PCIE_DMA_ACK_BASE + idx * NODE_SIZE;

		if (is_rc(obj)) {
			local += 3 * PCIE_DMA_ACK_BLOCK_SIZE;
			virt += 3 * PCIE_DMA_ACK_BLOCK_SIZE;
		} else {
			local += 2 * PCIE_DMA_ACK_BLOCK_SIZE;
			virt += 2 * PCIE_DMA_ACK_BLOCK_SIZE;
		}
		bus = local;
		writel(PCIE_DMA_DATA_FREE_ACK_CHECK, virt);
		break;
	default:
		dev_err(dev, "type = %d not support\n", type);
		return;
	}

	if (is_rc(obj)) {
		desc = table->descs;
		*(desc + 0) = (u32)(local & 0xffffffff);
		*(desc + 1) = (u32)(local >> 32);
		*(desc + 2) = (u32)(bus & 0xffffffff);
		*(desc + 3) = (u32)(bus >> 32);
		*(desc + 4) = 0;
		*(desc + 5) = 0;
		*(desc + 6) = buf_size;
		*(desc + 7) = 0;
		*(desc + 8) = 0;
		*(desc + 6) |= 1 << 24;
	} else {
		table->wr_enb.enb = 0x1;
		table->ctx_reg.ctrllo.lie = 0x1;
		table->ctx_reg.ctrllo.rie = 0x0;
		table->ctx_reg.ctrllo.td = 0x1;
		table->ctx_reg.ctrlhi.asdword = 0x0;
		table->ctx_reg.xfersize = buf_size;
		table->ctx_reg.sarptrlo = (u32)(local & 0xffffffff);
		table->ctx_reg.sarptrhi = (u32)(local >> 32);
		table->ctx_reg.darptrlo = (u32)(bus & 0xffffffff);
		table->ctx_reg.darptrhi = (u32)(bus >> 32);
		table->wr_weilo.weight0 = 0x0;
		table->start.stop = 0x0;
		table->start.chnl = PCIE_DMA_CHN0;
	}

	spin_lock_irqsave(&obj->tbl_list_lock, flags);
	list_add_tail(&table->tbl_node, &obj->tbl_list);
	spin_unlock_irqrestore(&obj->tbl_list_lock, flags);
}

static void rk_pcie_dma_trx_work(struct work_struct *work)
{
	unsigned long flags;
	struct dma_trx_obj *obj = container_of(work,
				struct dma_trx_obj, dma_trx_work);
	struct dma_table *table;

	while (!list_empty(&obj->tbl_list)) {
		table = list_first_entry(&obj->tbl_list, struct dma_table,
					 tbl_node);
		if (obj->dma_free) {
			obj->dma_free = false;
			spin_lock_irqsave(&obj->tbl_list_lock, flags);
			list_del_init(&table->tbl_node);
			spin_unlock_irqrestore(&obj->tbl_list_lock, flags);
			obj->cur = table;
			if (is_rc(obj))
				rk_pcie_start_dma_3399(obj);
			else
				rk_pcie_start_dma_1808(obj);
		}
	}
}

static int rk_pcie_scan_thread(void *data)
{
	struct dma_trx_obj *obj = (struct dma_trx_obj *)data;

	hrtimer_start(&obj->scan_timer,
		      ktime_set(0, 500 * 1000 * 1000), HRTIMER_MODE_REL);
	return 0;
}

static void rk_pcie_clear_ack(void *addr)
{
	writel(0x0, addr);
}

static enum hrtimer_restart rk_pcie_scan_timer(struct hrtimer *timer)
{
	unsigned int sdv;
	unsigned int idx;
	unsigned int sav;
	unsigned int suv;
	void *sda_base;
	void *scan_data_addr;
	void *scan_ack_addr;
	void *scan_user_addr;
	int i;
	bool need_ack = false;
	struct dma_trx_obj *obj = container_of(timer,
					struct dma_trx_obj, scan_timer);

	for (i = 0; i < PCIE_DMA_BUF_CNT; i++) {
		sda_base = obj->mem_base + PCIE_DMA_BUF_SIZE * i;

		if (is_rc(obj))
			scan_data_addr =  sda_base + PCIE_DMA_WR_BUF_SIZE;
		else
			scan_data_addr = sda_base;

		sdv = readl(scan_data_addr + PCIE_DMA_SET_DATA_CHECK_POS);
		idx = readl(scan_data_addr + PCIE_DMA_SET_LOCAL_IDX_POS);

		if (sdv == PCIE_DMA_DATA_CHECK) {
			if (!need_ack)
				need_ack = true;
			writel(0x0, scan_data_addr + PCIE_DMA_SET_DATA_CHECK_POS);
			set_bit(i, &obj->local_read_available);
			rk_pcie_prepare_dma(obj, idx, 0, 0, 0x4,
					PCIE_DMA_DATA_RCV_ACK);
		}
	}

	if (need_ack || !list_empty(&obj->tbl_list))
		queue_work(obj->dma_trx_wq, &obj->dma_trx_work);

	scan_ack_addr = obj->mem_base + PCIE_DMA_ACK_BASE;
	scan_user_addr = obj->mem_base + PCIE_DMA_ACK_BASE;

	if (is_rc(obj)) {
		scan_user_addr += PCIE_DMA_ACK_BLOCK_SIZE * 2;
	} else {
		scan_ack_addr += PCIE_DMA_ACK_BLOCK_SIZE;
		scan_user_addr += PCIE_DMA_ACK_BLOCK_SIZE * 3;
	}

	for (i = 0; i < PCIE_DMA_BUF_CNT; i++) {
		void *addr = scan_ack_addr + i * NODE_SIZE;

		sav = readl(addr);
		if (sav == PCIE_DMA_DATA_ACK_CHECK) {
			rk_pcie_clear_ack(addr);
			set_bit(i, &obj->local_write_available);
		}

		addr = scan_user_addr + i * NODE_SIZE;
		suv = readl(addr);
		if (suv == PCIE_DMA_DATA_FREE_ACK_CHECK) {
			rk_pcie_clear_ack(addr);
			set_bit(i, &obj->remote_write_available);
		}
	}

	if ((obj->local_write_available && obj->remote_write_available) ||
		obj->local_read_available) {
		wake_up(&obj->event_queue);
	}

	hrtimer_add_expires(&obj->scan_timer, ktime_set(0, 1 * 1000 * 1000));

	return HRTIMER_RESTART;
}

static int rk_pcie_misc_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *miscdev = filp->private_data;
	struct pcie_misc_dev *pcie_misc_dev = container_of(miscdev,
						 struct pcie_misc_dev, dev);

	filp->private_data = pcie_misc_dev->obj;

	mutex_lock(&pcie_misc_dev->obj->count_mutex);
	if (pcie_misc_dev->obj->ref_count++)
		goto already_opened;

	pcie_misc_dev->obj->loop_count = 0;
	pcie_misc_dev->obj->local_read_available = 0x0;
	pcie_misc_dev->obj->local_write_available = 0xff;
	pcie_misc_dev->obj->remote_write_available = 0xff;
	pcie_misc_dev->obj->dma_free = true;

	pr_info("Open pcie misc device success\n");

already_opened:
	mutex_unlock(&pcie_misc_dev->obj->count_mutex);
	return 0;
}

static int rk_pcie_misc_release(struct inode *inode, struct file *filp)
{
	struct dma_trx_obj *obj = filp->private_data;

	mutex_lock(&obj->count_mutex);
	if (--obj->ref_count)
		goto still_opened;

	pr_info("Close pcie misc device\n");

still_opened:
	mutex_unlock(&obj->count_mutex);
	return 0;
}

static int rk_pcie_misc_mmap(struct file *filp,
				     struct vm_area_struct *vma)
{
	struct dma_trx_obj *obj = filp->private_data;
	size_t size = vma->vm_end - vma->vm_start;
	int err;

	err = remap_pfn_range(vma, vma->vm_start,
			    __phys_to_pfn(obj->mem_start),
			    size, vma->vm_page_prot);
	if (err)
		return -EAGAIN;

	return 0;
}

static long rk_pcie_misc_ioctl(struct file *filp, unsigned int cmd,
					unsigned long arg)
{
	struct dma_trx_obj *obj = filp->private_data;
	struct device *dev = obj->dev;
	union pcie_dma_ioctl_param msg;
	union pcie_dma_ioctl_param msg_to_user;
	phys_addr_t addr;
	void __user *uarg = (void __user *)arg;
	int ret;

	if (copy_from_user(&msg, uarg, sizeof(msg)) != 0) {
		dev_err(dev, "failed to copy argument into kernel space\n");
		return -EFAULT;
	}

	switch (cmd) {
	case PCIE_DMA_START:
		test_and_clear_bit(msg.in.l_widx, &obj->local_write_available);
		test_and_clear_bit(msg.in.r_widx, &obj->remote_write_available);
		obj->loop_count++;
		break;
	case PCIE_DMA_GET_LOCAL_READ_BUFFER_INDEX:
		msg_to_user.lra = obj->local_read_available;
		addr = obj->mem_start;
		if (is_rc(obj))
			addr += PCIE_DMA_WR_BUF_SIZE;
		/* by kernel auto or by user to invalidate cache */
		dma_sync_single_for_cpu(dev, addr, PCIE_DMA_RD_BUF_SIZE,
					DMA_FROM_DEVICE);
		ret = copy_to_user(uarg, &msg_to_user, sizeof(msg));
		if (ret) {
			dev_err(dev, "failed to get read buffer index\n");
			return -EFAULT;
		}
		break;
	case PCIE_DMA_SET_LOCAL_READ_BUFFER_INDEX:
		test_and_clear_bit(msg.in.idx, &obj->local_read_available);
		break;
	case PCIE_DMA_GET_LOCAL_REMOTE_WRITE_BUFFER_INDEX:
		msg_to_user.out.lwa = obj->local_write_available;
		msg_to_user.out.rwa = obj->remote_write_available;
		ret = copy_to_user(uarg, &msg_to_user, sizeof(msg));
		if (ret) {
			dev_err(dev, "failed to get write buffer index\n");
			return -EFAULT;
		}
		break;
	case PCIE_DMA_SYNC_BUFFER_FOR_CPU:
		addr = obj->mem_start + msg.in.idx * PCIE_DMA_BUF_SIZE;
		if (is_rc(obj))
			addr += PCIE_DMA_WR_BUF_SIZE;
		dma_sync_single_for_cpu(dev, addr, PCIE_DMA_BUF_SIZE,
					DMA_FROM_DEVICE);
		break;
	case PCIE_DMA_WAIT_TRANSFER_COMPLETE:
		reinit_completion(&obj->done);
		ret = wait_for_completion_interruptible(&obj->done);
		if (WARN_ON(ret)) {
			pr_info("failed to wait complete\n");
			return ret;
		}

		obj->loop_count = 0;
		break;
	case PCIE_DMA_SET_LOOP_COUNT:
		obj->loop_count_threshold = msg.count;
		pr_info("threshold = %d\n", obj->loop_count_threshold);
		break;
	default:
		pr_info("%s, %d, cmd : %x not support\n", __func__, __LINE__,
			cmd);
		return -EFAULT;
	}

	if (cmd == PCIE_DMA_START ||
		cmd == PCIE_DMA_SET_LOCAL_READ_BUFFER_INDEX) {
		rk_pcie_prepare_dma(obj, msg.in.idx, msg.in.r_widx,
				    msg.in.l_widx, msg.in.size, msg.in.type);
		queue_work(obj->dma_trx_wq, &obj->dma_trx_work);
	}

	return 0;
}

static unsigned int rk_pcie_misc_poll(struct file *filp,
						poll_table *wait)
{
	struct dma_trx_obj *obj = filp->private_data;
	u32 lwa, rwa, lra;
	u32 ret = 0;

	poll_wait(filp, &obj->event_queue, wait);

	lwa = obj->local_write_available;
	rwa = obj->remote_write_available;
	if (lwa && rwa)
		ret = POLLOUT;

	lra = obj->local_read_available;
	if (lra)
		ret |= POLLIN;

	return ret;
}

static const struct file_operations rk_pcie_misc_fops = {
	.open		= rk_pcie_misc_open,
	.release	= rk_pcie_misc_release,
	.mmap		= rk_pcie_misc_mmap,
	.unlocked_ioctl	= rk_pcie_misc_ioctl,
	.poll		= rk_pcie_misc_poll,
};

static void rk_pcie_delete_misc(struct dma_trx_obj *obj)
{
	misc_deregister(&obj->pcie_dev->dev);
}

static int rk_pcie_add_misc(struct dma_trx_obj *obj)
{
	int ret;
	struct pcie_misc_dev *pcie_dev;

	pcie_dev = devm_kzalloc(obj->dev, sizeof(*pcie_dev), GFP_KERNEL);
	if (!pcie_dev)
		return -ENOMEM;

	pcie_dev->dev.minor = MISC_DYNAMIC_MINOR;
	pcie_dev->dev.name = "pcie-dev";
	pcie_dev->dev.fops = &rk_pcie_misc_fops;
	pcie_dev->dev.parent = NULL;

	ret = misc_register(&pcie_dev->dev);
	if (ret) {
		pr_err("pcie: failed to register misc device.\n");
		return ret;
	}

	pcie_dev->obj = obj;
	obj->pcie_dev = pcie_dev;

	pr_info("register misc device pcie-dev\n");

	return 0;
}

static void *rk_pcie_map_kernel(phys_addr_t start, size_t len)
{
	int i;
	void *vaddr;
	pgprot_t pgprot;
	phys_addr_t phys;
	int npages = PAGE_ALIGN(len) / PAGE_SIZE;
	struct page **p = vmalloc(sizeof(struct page *) * npages);

	if (!p)
		return NULL;

	pgprot = pgprot_noncached(PAGE_KERNEL);

	phys = start;
	for (i = 0; i < npages; i++) {
		p[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	vaddr = vmap(p, npages, VM_MAP, pgprot);
	vfree(p);

	return vaddr;
}

static void rk_pcie_unmap_kernel(void *vaddr)
{
	vunmap(vaddr);
}

static void rk_pcie_dma_table_free(struct dma_trx_obj *obj, int num)
{
	int i;
	struct dma_table *table;

	if (num > PCIE_DMA_TABLE_NUM)
		num = PCIE_DMA_TABLE_NUM;

	for (i = 0; i < num; i++) {
		table = obj->table[i];
		dma_free_coherent(obj->dev, PCIE_DMA_PARAM_SIZE,
			table->descs, table->phys_descs);
		kfree(table);
	}
}

static int rk_pcie_dma_table_alloc(struct dma_trx_obj *obj)
{
	int i;
	struct dma_table *table;

	for (i = 0; i < PCIE_DMA_TABLE_NUM; i++) {
		table = kzalloc(sizeof(*table), GFP_KERNEL);
		if (!table)
			goto free_table;

		table->descs = dma_alloc_coherent(obj->dev, PCIE_DMA_PARAM_SIZE,
				&table->phys_descs, GFP_KERNEL | __GFP_ZERO);
		if (!table->descs) {
			kfree(table);
			goto free_table;
		}

		if (is_rc(obj))
			table->dir = DMA_TO_BUS;

		table->chn = PCIE_DMA_CHN0;
		INIT_LIST_HEAD(&table->tbl_node);
		obj->table[i] = table;
	}

	return 0;

free_table:
	rk_pcie_dma_table_free(obj, i);
	dev_err(obj->dev, "Failed to alloc dma table\n");

	return -ENOMEM;
}

#ifdef CONFIG_DEBUG_FS
static int rk_pcie_debugfs_trx_show(struct seq_file *s, void *v)
{
	struct dma_trx_obj *dma_obj = s->private;
	bool list = list_empty(&dma_obj->tbl_list);

	seq_printf(s, "irq_num = %ld, loop_count = %d,",
			dma_obj->irq_num, dma_obj->loop_count);
	seq_printf(s, "loop_threshold = %d,",
			dma_obj->loop_count_threshold);
	seq_printf(s, "lwa = %lx, rwa = %lx, lra = %lx,",
			dma_obj->local_write_available,
			dma_obj->remote_write_available,
			dma_obj->local_read_available);
	seq_printf(s, "list : (%s), dma chn : (%s)\n",
			list ? "empty" : "not empty",
			dma_obj->dma_free ? "free" : "busy");

	return 0;
}

static int rk_pcie_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, rk_pcie_debugfs_trx_show, inode->i_private);
}

static const struct file_operations rk_pcie_debugfs_fops = {
	.owner = THIS_MODULE,
	.open = rk_pcie_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

struct dma_trx_obj *rk_pcie_dma_obj_probe(struct device *dev)
{
	int ret;
	int busno;
	struct device_node *np = dev->of_node;
	struct device_node *mem;
	struct resource reg;
	struct dma_trx_obj *obj;

	obj = devm_kzalloc(dev, sizeof(struct dma_trx_obj), GFP_KERNEL);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	obj->dev = dev;

	ret = of_property_read_u32(np, "busno", &busno);
	if (ret < 0) {
		dev_err(dev, "missing \"busno\" property\n");
		return ERR_PTR(ret);
	}

	obj->busno = busno;

	mem = of_parse_phandle(np, "memory-region", 0);
	if (!mem) {
		dev_err(dev, "missing \"memory-region\" property\n");
		return ERR_PTR(-ENODEV);
	}

	ret = of_address_to_resource(mem, 0, &reg);
	if (ret < 0) {
		dev_err(dev, "missing \"reg\" property\n");
		return ERR_PTR(-ENODEV);
	}

	obj->mem_start = reg.start;
	obj->mem_size = resource_size(&reg);
	obj->mem_base = rk_pcie_map_kernel(obj->mem_start, obj->mem_size);

	if (!obj->mem_base)
		return ERR_PTR(-ENOMEM);

	ret = rk_pcie_dma_table_alloc(obj);
	if (ret)
		return ERR_PTR(-ENOMEM);

	obj->dma_trx_wq = create_singlethread_workqueue("dma_trx_wq");
	INIT_WORK(&obj->dma_trx_work, rk_pcie_dma_trx_work);

	INIT_LIST_HEAD(&obj->tbl_list);
	spin_lock_init(&obj->tbl_list_lock);

	init_waitqueue_head(&obj->event_queue);

	hrtimer_init_on_stack(&obj->scan_timer, CLOCK_MONOTONIC,
				HRTIMER_MODE_REL);
	obj->scan_timer.function = rk_pcie_scan_timer;
	obj->scan_thread = kthread_run(rk_pcie_scan_thread, (void *)obj,
				"scan_thread");
	if (!obj->scan_thread) {
		dev_err(dev, "kthread_run failed\n");
		obj = ERR_PTR(-EINVAL);
		goto free_dma_table;
	}

	obj->irq_num = 0;
	obj->loop_count_threshold = 0;
	obj->ref_count = 0;
	init_completion(&obj->done);

	mutex_init(&obj->count_mutex);
	rk_pcie_add_misc(obj);

#ifdef CONFIG_DEBUG_FS
	obj->pcie_root = debugfs_create_dir("pcie", NULL);
	if (!obj->pcie_root) {
		obj = ERR_PTR(-EINVAL);
		goto free_dma_table;
	}

	debugfs_create_file("pcie_trx", 0644, obj->pcie_root, obj,
			&rk_pcie_debugfs_fops);
#endif

	return obj;
free_dma_table:
	rk_pcie_dma_table_free(obj, PCIE_DMA_TABLE_NUM);
	return obj;
}
EXPORT_SYMBOL_GPL(rk_pcie_dma_obj_probe);

void rk_pcie_dma_obj_remove(struct dma_trx_obj *obj)
{
	hrtimer_cancel(&obj->scan_timer);
	destroy_hrtimer_on_stack(&obj->scan_timer);
	rk_pcie_delete_misc(obj);
	rk_pcie_unmap_kernel(obj->mem_base);
	rk_pcie_dma_table_free(obj, PCIE_DMA_TABLE_NUM);
	destroy_workqueue(obj->dma_trx_wq);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(obj->pcie_root);
#endif
}
EXPORT_SYMBOL_GPL(rk_pcie_dma_obj_remove);
