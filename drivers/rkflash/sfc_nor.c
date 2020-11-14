// SPDX-License-Identifier: GPL-2.0

/* Copyright (c) 2018 Rockchip Electronics Co. Ltd. */

#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/kernel.h>

#include "rkflash_debug.h"
#include "sfc_nor.h"

static struct flash_info spi_flash_tbl[] = {
	/* GD25Q32B */
	{ 0xc84016, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 13, 9, 0 },
	/* GD25Q64B */
	{ 0xc84017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 14, 9, 0 },
	/* GD25Q127C and GD25Q128C*/
	{ 0xc84018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 15, 9, 0 },
	/* GD25Q256B/C/D */
	{ 0xc84019, 128, 8, 0x13, 0x12, 0x6C, 0x3E, 0x21, 0xDC, 0x1C, 16, 6, 0 },
	/* GD25Q512MC */
	{ 0xc84020, 128, 8, 0x13, 0x12, 0x6C, 0x3E, 0x21, 0xDC, 0x1C, 17, 6, 0 },
	/* 25Q64JVSSIQ */
	{ 0xef4017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 9, 0 },
	/* 25Q128FV and 25Q128JV*/
	{ 0xef4018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 15, 9, 0 },
	/* 25Q256F/J */
	{ 0xef4019, 128, 8, 0x13, 0x02, 0x6C, 0x32, 0x20, 0xD8, 0x3C, 16, 9, 0 },
	/* 25Q64FWSSIG */
	{ 0xef6017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 9, 0 },
	/* MX25L6433F */
	{ 0xc22017, 128, 8, 0x03, 0x02, 0x6B, 0x38, 0x20, 0xD8, 0x0E, 14, 6, 0 },
	/* MX25L12835E/F MX25L12833FMI-10G */
	{ 0xc22018, 128, 8, 0x03, 0x02, 0x6B, 0x38, 0x20, 0xD8, 0x0E, 15, 6, 0 },
	/* MX25L25635E/F MX25L25645G MX25L25645GMI-08G*/
	{ 0xc22019, 128, 8, 0x13, 0x12, 0x6C, 0x38, 0x21, 0xDC, 0x3E, 16, 6, 0 },
	/* MX25L51245GMI */
	{ 0xc2201a, 128, 8, 0x13, 0x12, 0x6C, 0x38, 0x21, 0xDC, 0x3E, 17, 6, 0 },
	/* XM25QH32C */
	{ 0x204016, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 13, 9, 0 },
	/* XM25QH64B */
	{ 0x206017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 14, 6, 0 },
	/* XM25QH128B */
	{ 0x206018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 15, 6, 0 },
	/* XM25QH(QU)256B */
	{ 0x206019, 128, 8, 0x13, 0x12, 0x6C, 0x3E, 0x21, 0xDC, 0x1D, 16, 6, 0 },
	/* XM25QH64A */
	{ 0x207017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 0, 0 },
	/* XT25F128A XM25QH128A */
	{ 0x207018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x00, 15, 0, 0 },
	/* XT25F64BSSIGU-5 */
	{ 0x0b4017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 14, 9, 0 },
	/* XT25F128BSSIGU */
	{ 0x0b4018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 15, 9, 0 },
	/* EN25QH128A */
	{ 0x1c7018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 15, 0, 0 },
	/* EN25S32A */
	{ 0x1c3816, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 13, 0, 0 },
	/* EN25S64A */
	{ 0x1c3817, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 0, 0 },
	/* P25Q64H */
	{ 0x856017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 9, 0 },
	/* EN25QH256A */
	{ 0x1c7019, 128, 8, 0x13, 0x12, 0x6C, 0x34, 0x21, 0xDC, 0x3C, 16, 0, 0 },
	/* FM25Q64A */
	{ 0xf83217, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0D, 14, 9, 0 },
	/* ZB25VQ64 */
	{ 0x5e4017, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 14, 9, 0 },
	/* ZB25VQ128 */
	{ 0x5e4018, 128, 8, 0x03, 0x02, 0x6B, 0x32, 0x20, 0xD8, 0x0C, 15, 9, 0 },
};

static int snor_write_en(void)
{
	int ret;
	union SFCCMD_DATA     sfcmd;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_WRITE_EN;

	ret = sfc_request(sfcmd.d32, 0, 0, NULL);

	return ret;
}

int snor_reset_device(void)
{
	int ret;
	union SFCCMD_DATA sfcmd;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_ENABLE_RESER;
	sfc_request(sfcmd.d32, 0, 0, NULL);

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_RESET_DEVICE;
	ret = sfc_request(sfcmd.d32, 0, 0, NULL);
	/* tRST=30us , delay 1ms here */
	mdelay(1);
	return ret;
}

static int snor_enter_4byte_mode(void)
{
	int ret;
	union SFCCMD_DATA sfcmd;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_ENTER_4BYTE_MODE;

	ret = sfc_request(sfcmd.d32, 0, 0, NULL);
	return ret;
}

static int snor_read_status(u32 reg_index, u8 *status)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	u8 read_stat_cmd[] = {CMD_READ_STATUS,
				CMD_READ_STATUS2, CMD_READ_STATUS3};
	sfcmd.d32 = 0;
	sfcmd.b.cmd = read_stat_cmd[reg_index];
	sfcmd.b.datasize = 1;

	ret = sfc_request(sfcmd.d32, 0, 0, status);

	return ret;
}

static int snor_wait_busy(int timeout)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	int i;
	u32 status;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_READ_STATUS;
	sfcmd.b.datasize = 1;

	for (i = 0; i < timeout; i++) {
		ret = sfc_request(sfcmd.d32, 0, 0, &status);
		if (ret != SFC_OK)
			return ret;

		if ((status & 0x01) == 0)
			return SFC_OK;

		sfc_delay(1);
	}
	rkflash_print_error("%s  error %x\n", __func__, timeout);

	return SFC_BUSY_TIMEOUT;
}

static int snor_write_status2(u32 reg_index, u8 status)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	u8 status2[2];
	u8 read_index;

	status2[reg_index] = status;
	read_index = (reg_index == 0) ? 2 : 0;
	ret = snor_read_status(read_index, &status2[read_index]);
	if (ret != SFC_OK)
		return ret;

	snor_write_en();

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_WRITE_STATUS;
	sfcmd.b.datasize = 2;
	sfcmd.b.rw = SFC_WRITE;

	ret = sfc_request(sfcmd.d32, 0, 0, &status2[0]);
	if (ret != SFC_OK)
		return ret;

	ret = snor_wait_busy(10000);    /* 10ms */

	return ret;
}

static int snor_write_status1(u32 reg_index, u8 status)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	u8 status2[2];
	u8 read_index;

	status2[reg_index] = status;
	read_index = (reg_index == 0) ? 1 : 0;
	ret = snor_read_status(read_index, &status2[read_index]);
	if (ret != SFC_OK)
		return ret;

	snor_write_en();

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_WRITE_STATUS;
	sfcmd.b.datasize = 2;
	sfcmd.b.rw = SFC_WRITE;

	ret = sfc_request(sfcmd.d32, 0, 0, &status2[0]);
	if (ret != SFC_OK)
		return ret;

	ret = snor_wait_busy(10000);    /* 10ms */

	return ret;
}

static int snor_write_status(u32 reg_index, u8 status)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	u8 write_stat_cmd[] = {CMD_WRITE_STATUS,
			       CMD_WRITE_STATUS2, CMD_WRITE_STATUS3};
	snor_write_en();
	sfcmd.d32 = 0;
	sfcmd.b.cmd = write_stat_cmd[reg_index];
	sfcmd.b.datasize = 1;
	sfcmd.b.rw = SFC_WRITE;

	ret = sfc_request(sfcmd.d32, 0, 0, &status);
	if (ret != SFC_OK)
		return ret;

	ret = snor_wait_busy(10000);    /* 10ms */

	return ret;
}

int snor_erase(struct SFNOR_DEV *p_dev,
	       u32 addr,
	       enum NOR_ERASE_TYPE erase_type)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	int timeout[] = {400, 2000, 40000};   /* ms */

	if (erase_type > ERASE_CHIP)
		return SFC_PARAM_ERR;

	sfcmd.d32 = 0;
	if (erase_type == ERASE_BLOCK64K)
		sfcmd.b.cmd = p_dev->blk_erase_cmd;
	else if (erase_type == ERASE_SECTOR)
		sfcmd.b.cmd = p_dev->sec_erase_cmd;
	else
		sfcmd.b.cmd = CMD_CHIP_ERASE;

	sfcmd.b.addrbits = (erase_type != ERASE_CHIP) ?
				SFC_ADDR_24BITS : SFC_ADDR_0BITS;
	if (p_dev->addr_mode == ADDR_MODE_4BYTE && erase_type != ERASE_CHIP)
		sfcmd.b.addrbits = SFC_ADDR_32BITS;

	snor_write_en();

	ret = sfc_request(sfcmd.d32, 0, addr, NULL);
	if (ret != SFC_OK)
		return ret;

	ret = snor_wait_busy(timeout[erase_type] * 1000);
	return ret;
}

int snor_prog_page(struct SFNOR_DEV *p_dev,
		   u32 addr,
		   void *p_data,
		   u32 size)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	union SFCCTRL_DATA sfctrl;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = p_dev->prog_cmd;
	sfcmd.b.addrbits = SFC_ADDR_24BITS;
	sfcmd.b.datasize = size;
	sfcmd.b.rw = SFC_WRITE;

	sfctrl.d32 = 0;
	sfctrl.b.datalines = p_dev->prog_lines;
	sfctrl.b.enbledma = 0;
	if (p_dev->prog_cmd == CMD_PAGE_PROG_A4)
		sfctrl.b.addrlines = SFC_4BITS_LINE;

	if (p_dev->addr_mode == ADDR_MODE_4BYTE)
		sfcmd.b.addrbits = SFC_ADDR_32BITS;

	snor_write_en();

	ret = sfc_request(sfcmd.d32, sfctrl.d32, addr, p_data);
	if (ret != SFC_OK)
		return ret;

	ret = snor_wait_busy(10000);

	return ret;
}

static int snor_prog(struct SFNOR_DEV *p_dev, u32 addr, void *p_data, u32 size)
{
	int ret = SFC_OK;
	u32 page_size, len;
	u8 *p_buf =  (u8 *)p_data;

	page_size = NOR_PAGE_SIZE;
	while (size) {
		len = page_size < size ? page_size : size;
		ret = snor_prog_page(p_dev, addr, p_buf, len);
		if (ret != SFC_OK)
			return ret;

		size -= len;
		addr += len;
		p_buf += len;
	}

	return ret;
}

static int snor_enable_QE(struct SFNOR_DEV *p_dev)
{
	int ret = SFC_OK;
	int reg_index;
	int bit_offset;
	u8 status;

	if (p_dev->manufacturer == MID_GIGADEV ||
	    p_dev->manufacturer == MID_WINBOND ||
	    p_dev->manufacturer == MID_XTX ||
	    p_dev->manufacturer == MID_MACRONIX ||
	    p_dev->manufacturer == MID_PUYA ||
	    p_dev->manufacturer == MID_XMC ||
	    p_dev->manufacturer == MID_DOSILICON ||
	    p_dev->manufacturer == MID_ZBIT) {
		reg_index = p_dev->QE_bits >> 3;
		bit_offset = p_dev->QE_bits & 0x7;
		ret = snor_read_status(reg_index, &status);
		if (ret != SFC_OK)
			return ret;

		if (status & (1 << bit_offset))   /* is QE bit set */
			return SFC_OK;

		status |= (1 << bit_offset);
		return p_dev->write_status(reg_index, status);
	}

	return ret;
}

int snor_disable_QE(struct SFNOR_DEV *p_dev)
{
	int ret = SFC_OK;
	int reg_index;
	int bit_offset;
	u8 status;

	if (p_dev->manufacturer == MID_GIGADEV ||
	    p_dev->manufacturer == MID_WINBOND ||
	    p_dev->manufacturer == MID_XTX ||
	    p_dev->manufacturer == MID_MACRONIX ||
	    p_dev->manufacturer == MID_PUYA ||
	    p_dev->manufacturer == MID_XMC ||
	    p_dev->manufacturer == MID_DOSILICON ||
	    p_dev->manufacturer == MID_ZBIT) {
		reg_index = p_dev->QE_bits >> 3;
		bit_offset = p_dev->QE_bits & 0x7;
		ret = snor_read_status(reg_index, &status);
		if (ret != SFC_OK)
			return ret;

		if (!(status & (1 << bit_offset)))
			return SFC_OK;

		status &= ~(1 << bit_offset);
		return p_dev->write_status(reg_index, status);
	}

	return ret;
}

int snor_read_data(struct SFNOR_DEV *p_dev,
		   u32 addr,
		   void *p_data,
		   u32 size)
{
	int ret;
	union SFCCMD_DATA sfcmd;
	union SFCCTRL_DATA sfctrl;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = p_dev->read_cmd;
	sfcmd.b.datasize = size;
	sfcmd.b.addrbits = SFC_ADDR_24BITS;

	sfctrl.d32 = 0;
	sfctrl.b.datalines = p_dev->read_lines;
	if (!(size & 0x3) && size >= 4)
		sfctrl.b.enbledma = 0;

	if (p_dev->read_cmd == CMD_FAST_READ_X1 ||
	    p_dev->read_cmd == CMD_FAST_READ_X4 ||
	    p_dev->read_cmd == CMD_FAST_READ_X2 ||
	    p_dev->read_cmd == CMD_FAST_4READ_X4) {
		sfcmd.b.dummybits = 8;
	} else if (p_dev->read_cmd == CMD_FAST_READ_A4) {
		sfcmd.b.addrbits = SFC_ADDR_32BITS;
		addr = (addr << 8) | 0xFF;	/* Set M[7:0] = 0xFF */
		sfcmd.b.dummybits = 4;
		sfctrl.b.addrlines = SFC_4BITS_LINE;
	}

	if (p_dev->addr_mode == ADDR_MODE_4BYTE)
		sfcmd.b.addrbits = SFC_ADDR_32BITS;

	ret = sfc_request(sfcmd.d32, sfctrl.d32, addr, p_data);

	return ret;
}

int snor_read(struct SFNOR_DEV *p_dev, u32 sec, u32 n_sec, void *p_data)
{
	int ret = SFC_OK;
	u32 addr, size, len;
	u8 *p_buf =  (u8 *)p_data;

	if ((sec + n_sec) > p_dev->capacity)
		return SFC_PARAM_ERR;

	addr = sec << 9;
	size = n_sec << 9;
	while (size) {
		len = size < SFC_MAX_IOSIZE ? size : SFC_MAX_IOSIZE;
		ret = snor_read_data(p_dev, addr, p_buf, len);
		if (ret != SFC_OK) {
			rkflash_print_error("snor_read_data %x ret= %x\n",
					    addr >> 9, ret);
			goto out;
		}

		size -= len;
		addr += len;
		p_buf += len;
	}
out:
	if (!ret)
		ret = n_sec;

	return ret;
}

int snor_write(struct SFNOR_DEV *p_dev, u32 sec, u32 n_sec, void *p_data)
{
	int ret = SFC_OK;
	u32 len, blk_size, offset;
	u8 *p_buf =  (u8 *)p_data;
	u32 total_sec = n_sec;

	if ((sec + n_sec) > p_dev->capacity)
		return SFC_PARAM_ERR;

	while (n_sec) {
		if (sec < 512 || sec >= p_dev->capacity  - 512)
			blk_size = 8;
		else
			blk_size = p_dev->blk_size;

		offset = (sec & (blk_size - 1));
		if (!offset) {
			ret = snor_erase(p_dev, sec << 9, (blk_size == 8) ?
				ERASE_SECTOR : ERASE_BLOCK64K);
			if (ret != SFC_OK) {
				rkflash_print_error("snor_erase %x ret= %x\n",
						    sec, ret);
				goto out;
			}
		}
		len = (blk_size - offset) < n_sec ?
		      (blk_size - offset) : n_sec;
		ret = snor_prog(p_dev, sec << 9, p_buf, len << 9);
		if (ret != SFC_OK) {
			rkflash_print_error("snor_prog %x ret= %x\n", sec, ret);
			goto out;
		}
		n_sec -= len;
		sec += len;
		p_buf += len << 9;
	}
out:
	if (!ret)
		ret = total_sec;

	return ret;
}

int snor_read_id(u8 *data)
{
	int ret;
	union SFCCMD_DATA     sfcmd;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_READ_JEDECID;
	sfcmd.b.datasize = 3;

	ret = sfc_request(sfcmd.d32, 0, 0, data);

	return ret;
}

static int snor_read_parameter(u32 addr, u8 *data)
{
	int ret;
	union SFCCMD_DATA     sfcmd;

	sfcmd.d32 = 0;
	sfcmd.b.cmd = CMD_READ_PARAMETER;
	sfcmd.b.datasize = 1;
	sfcmd.b.addrbits = SFC_ADDR_24BITS;
	sfcmd.b.dummybits = 8;

	ret = sfc_request(sfcmd.d32, 0, addr, data);

	return ret;
}

u32 snor_get_capacity(struct SFNOR_DEV *p_dev)
{
	return p_dev->capacity;
}

static struct flash_info *snor_get_flash_info(u8 *flash_id)
{
	u32 i;
	u32 id = (flash_id[0] << 16) | (flash_id[1] << 8) | (flash_id[2] << 0);

	for (i = 0; i < ARRAY_SIZE(spi_flash_tbl); i++) {
		if (spi_flash_tbl[i].id == id)
			return &spi_flash_tbl[i];
	}
	return NULL;
}

/* Adjust flash info in ram base on parameter */
static void *snor_flash_info_adjust(struct flash_info *spi_flash_info)
{
	u32 addr;
	u8 para_version;

	if (spi_flash_info->id == 0xc84019) {
		addr = 0x09;
		snor_read_parameter(addr, &para_version);
		if (para_version == 0x06) {
			spi_flash_info->QE_bits = 9;
			spi_flash_info->prog_cmd_4 = 0x34;
		}
	}
	return 0;
}

int snor_init(struct SFNOR_DEV *p_dev)
{
	struct flash_info *g_spi_flash_info;
	u32 i;
	u8 id_byte[5];

	if (!p_dev)
		return SFC_PARAM_ERR;

	memset(p_dev, 0, sizeof(struct SFNOR_DEV));
	snor_read_id(id_byte);
	rkflash_print_error("sfc nor id: %x %x %x\n",
			    id_byte[0], id_byte[1], id_byte[2]);
	if (0xFF == id_byte[0] || 0x00 == id_byte[0])
		return SFC_ERROR;

	p_dev->manufacturer = id_byte[0];
	p_dev->mem_type = id_byte[1];

	g_spi_flash_info = snor_get_flash_info(id_byte);
	if (g_spi_flash_info) {
		snor_flash_info_adjust(g_spi_flash_info);
		p_dev->capacity = 1 << g_spi_flash_info->density;
		p_dev->blk_size = g_spi_flash_info->block_size;
		p_dev->page_size = NOR_SECS_PAGE;
		p_dev->read_cmd = g_spi_flash_info->read_cmd;
		p_dev->prog_cmd = g_spi_flash_info->prog_cmd;
		p_dev->sec_erase_cmd = g_spi_flash_info->sector_erase_cmd;
		p_dev->blk_erase_cmd = g_spi_flash_info->block_erase_cmd;
		p_dev->prog_lines = DATA_LINES_X1;
		p_dev->read_lines = DATA_LINES_X1;
		p_dev->QE_bits = g_spi_flash_info->QE_bits;
		p_dev->addr_mode = ADDR_MODE_3BYTE;

		i = g_spi_flash_info->feature & FEA_READ_STATUE_MASK;
		if (i == 0)
			p_dev->write_status = snor_write_status;
		else if (i == 1)
			p_dev->write_status = snor_write_status1;
		else if (i == 2)
			p_dev->write_status = snor_write_status2;
		if (g_spi_flash_info->feature & FEA_4BIT_READ) {
			if (snor_enable_QE(p_dev) == SFC_OK) {
				p_dev->read_lines = DATA_LINES_X4;
				p_dev->read_cmd = g_spi_flash_info->read_cmd_4;
			}
		}
		if (g_spi_flash_info->feature & FEA_4BIT_PROG &&
		    p_dev->read_lines == DATA_LINES_X4) {
			p_dev->prog_lines = DATA_LINES_X4;
			p_dev->prog_cmd = g_spi_flash_info->prog_cmd_4;
		}

		if (g_spi_flash_info->feature & FEA_4BYTE_ADDR)
			p_dev->addr_mode = ADDR_MODE_4BYTE;

		if ((g_spi_flash_info->feature & FEA_4BYTE_ADDR_MODE))
			snor_enter_4byte_mode();
	} else {
		p_dev->capacity = 1 << id_byte[2] >> 3;
		p_dev->QE_bits = 0;
		p_dev->blk_size = NOR_SECS_BLK;
		p_dev->page_size = NOR_SECS_PAGE;
		p_dev->read_cmd = CMD_READ_DATA;
		p_dev->prog_cmd = CMD_PAGE_PROG;
		p_dev->sec_erase_cmd = CMD_SECTOR_ERASE;
		p_dev->blk_erase_cmd = CMD_BLOCK_ERASE;
		p_dev->prog_lines = DATA_LINES_X1;
		p_dev->read_lines = DATA_LINES_X1;
		p_dev->write_status = snor_write_status;
	}

	rkflash_print_info("addr_mode: %x\n", p_dev->addr_mode);
	rkflash_print_info("read_lines: %x\n", p_dev->read_lines);
	rkflash_print_info("prog_lines: %x\n", p_dev->prog_lines);
	rkflash_print_info("read_cmd: %x\n", p_dev->read_cmd);
	rkflash_print_info("prog_cmd: %x\n", p_dev->prog_cmd);
	rkflash_print_info("blk_erase_cmd: %x\n", p_dev->blk_erase_cmd);
	rkflash_print_info("sec_erase_cmd: %x\n", p_dev->sec_erase_cmd);

	return SFC_OK;
}

