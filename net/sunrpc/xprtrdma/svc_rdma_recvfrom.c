/*
 * Copyright (c) 2016, 2017 Oracle. All rights reserved.
 * Copyright (c) 2014 Open Grid Computing, Inc. All rights reserved.
 * Copyright (c) 2005-2006 Network Appliance, Inc. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the BSD-type
 * license below:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *      Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *      Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 *      Neither the name of the Network Appliance, Inc. nor the names of
 *      its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Tom Tucker <tom@opengridcomputing.com>
 */

/* Operation
 *
 * The main entry point is svc_rdma_recvfrom. This is called from
 * svc_recv when the transport indicates there is incoming data to
 * be read. "Data Ready" is signaled when an RDMA Receive completes,
 * or when a set of RDMA Reads complete.
 *
 * An svc_rqst is passed in. This structure contains an array of
 * free pages (rq_pages) that will contain the incoming RPC message.
 *
 * Short messages are moved directly into svc_rqst::rq_arg, and
 * the RPC Call is ready to be processed by the Upper Layer.
 * svc_rdma_recvfrom returns the length of the RPC Call message,
 * completing the reception of the RPC Call.
 *
 * However, when an incoming message has Read chunks,
 * svc_rdma_recvfrom must post RDMA Reads to pull the RPC Call's
 * data payload from the client. svc_rdma_recvfrom sets up the
 * RDMA Reads using pages in svc_rqst::rq_pages, which are
 * transferred to an svc_rdma_op_ctxt for the duration of the
 * I/O. svc_rdma_recvfrom then returns zero, since the RPC message
 * is still not yet ready.
 *
 * When the Read chunk payloads have become available on the
 * server, "Data Ready" is raised again, and svc_recv calls
 * svc_rdma_recvfrom again. This second call may use a different
 * svc_rqst than the first one, thus any information that needs
 * to be preserved across these two calls is kept in an
 * svc_rdma_op_ctxt.
 *
 * The second call to svc_rdma_recvfrom performs final assembly
 * of the RPC Call message, using the RDMA Read sink pages kept in
 * the svc_rdma_op_ctxt. The xdr_buf is copied from the
 * svc_rdma_op_ctxt to the second svc_rqst. The second call returns
 * the length of the completed RPC Call message.
 *
 * Page Management
 *
 * Pages under I/O must be transferred from the first svc_rqst to an
 * svc_rdma_op_ctxt before the first svc_rdma_recvfrom call returns.
 *
 * The first svc_rqst supplies pages for RDMA Reads. These are moved
 * from rqstp::rq_pages into ctxt::pages. The consumed elements of
 * the rq_pages array are set to NULL and refilled with the first
 * svc_rdma_recvfrom call returns.
 *
 * During the second svc_rdma_recvfrom call, RDMA Read sink pages
 * are transferred from the svc_rdma_op_ctxt to the second svc_rqst
 * (see rdma_read_complete() below).
 */

#include <asm/unaligned.h>
#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include <linux/spinlock.h>

#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/debug.h>
#include <linux/sunrpc/rpc_rdma.h>
#include <linux/sunrpc/svc_rdma.h>

#define RPCDBG_FACILITY	RPCDBG_SVCXPRT

/*
 * Replace the pages in the rq_argpages array with the pages from the SGE in
 * the RDMA_RECV completion. The SGL should contain full pages up until the
 * last one.
 */
static void rdma_build_arg_xdr(struct svc_rqst *rqstp,
			       struct svc_rdma_op_ctxt *ctxt,
			       u32 byte_count)
{
	struct page *page;
	u32 bc;
	int sge_no;

	/* Swap the page in the SGE with the page in argpages */
	page = ctxt->pages[0];
	put_page(rqstp->rq_pages[0]);
	rqstp->rq_pages[0] = page;

	/* Set up the XDR head */
	rqstp->rq_arg.head[0].iov_base = page_address(page);
	rqstp->rq_arg.head[0].iov_len =
		min_t(size_t, byte_count, ctxt->sge[0].length);
	rqstp->rq_arg.len = byte_count;
	rqstp->rq_arg.buflen = byte_count;

	/* Compute bytes past head in the SGL */
	bc = byte_count - rqstp->rq_arg.head[0].iov_len;

	/* If data remains, store it in the pagelist */
	rqstp->rq_arg.page_len = bc;
	rqstp->rq_arg.page_base = 0;

	sge_no = 1;
	while (bc && sge_no < ctxt->count) {
		page = ctxt->pages[sge_no];
		put_page(rqstp->rq_pages[sge_no]);
		rqstp->rq_pages[sge_no] = page;
		bc -= min_t(u32, bc, ctxt->sge[sge_no].length);
		sge_no++;
	}
	rqstp->rq_respages = &rqstp->rq_pages[sge_no];
	rqstp->rq_next_page = rqstp->rq_respages + 1;

	/* If not all pages were used from the SGL, free the remaining ones */
	bc = sge_no;
	while (sge_no < ctxt->count) {
		page = ctxt->pages[sge_no++];
		put_page(page);
	}
	ctxt->count = bc;

	/* Set up tail */
	rqstp->rq_arg.tail[0].iov_base = NULL;
	rqstp->rq_arg.tail[0].iov_len = 0;
}

/* This accommodates the largest possible Write chunk,
 * in one segment.
 */
#define MAX_BYTES_WRITE_SEG	((u32)(RPCSVC_MAXPAGES << PAGE_SHIFT))

/* This accommodates the largest possible Position-Zero
 * Read chunk or Reply chunk, in one segment.
 */
#define MAX_BYTES_SPECIAL_SEG	((u32)((RPCSVC_MAXPAGES + 2) << PAGE_SHIFT))

/* Sanity check the Read list.
 *
 * Implementation limits:
 * - This implementation supports only one Read chunk.
 *
 * Sanity checks:
 * - Read list does not overflow buffer.
 * - Segment size limited by largest NFS data payload.
 *
 * The segment count is limited to how many segments can
 * fit in the transport header without overflowing the
 * buffer. That's about 40 Read segments for a 1KB inline
 * threshold.
 *
 * Returns pointer to the following Write list.
 */
static __be32 *xdr_check_read_list(__be32 *p, const __be32 *end)
{
	u32 position;
	bool first;

	first = true;
	while (*p++ != xdr_zero) {
		if (first) {
			position = be32_to_cpup(p++);
			first = false;
		} else if (be32_to_cpup(p++) != position) {
			return NULL;
		}
		p++;	/* handle */
		if (be32_to_cpup(p++) > MAX_BYTES_SPECIAL_SEG)
			return NULL;
		p += 2;	/* offset */

		if (p > end)
			return NULL;
	}
	return p;
}

/* The segment count is limited to how many segments can
 * fit in the transport header without overflowing the
 * buffer. That's about 60 Write segments for a 1KB inline
 * threshold.
 */
static __be32 *xdr_check_write_chunk(__be32 *p, const __be32 *end,
				     u32 maxlen)
{
	u32 i, segcount;

	segcount = be32_to_cpup(p++);
	for (i = 0; i < segcount; i++) {
		p++;	/* handle */
		if (be32_to_cpup(p++) > maxlen)
			return NULL;
		p += 2;	/* offset */

		if (p > end)
			return NULL;
	}

	return p;
}

/* Sanity check the Write list.
 *
 * Implementation limits:
 * - This implementation supports only one Write chunk.
 *
 * Sanity checks:
 * - Write list does not overflow buffer.
 * - Segment size limited by largest NFS data payload.
 *
 * Returns pointer to the following Reply chunk.
 */
static __be32 *xdr_check_write_list(__be32 *p, const __be32 *end)
{
	u32 chcount;

	chcount = 0;
	while (*p++ != xdr_zero) {
		p = xdr_check_write_chunk(p, end, MAX_BYTES_WRITE_SEG);
		if (!p)
			return NULL;
		if (chcount++ > 1)
			return NULL;
	}
	return p;
}

/* Sanity check the Reply chunk.
 *
 * Sanity checks:
 * - Reply chunk does not overflow buffer.
 * - Segment size limited by largest NFS data payload.
 *
 * Returns pointer to the following RPC header.
 */
static __be32 *xdr_check_reply_chunk(__be32 *p, const __be32 *end)
{
	if (*p++ != xdr_zero) {
		p = xdr_check_write_chunk(p, end, MAX_BYTES_SPECIAL_SEG);
		if (!p)
			return NULL;
	}
	return p;
}

/* On entry, xdr->head[0].iov_base points to first byte in the
 * RPC-over-RDMA header.
 *
 * On successful exit, head[0] points to first byte past the
 * RPC-over-RDMA header. For RDMA_MSG, this is the RPC message.
 * The length of the RPC-over-RDMA header is returned.
 *
 * Assumptions:
 * - The transport header is entirely contained in the head iovec.
 */
static int svc_rdma_xdr_decode_req(struct xdr_buf *rq_arg)
{
	__be32 *p, *end, *rdma_argp;
	unsigned int hdr_len;
	char *proc;

	/* Verify that there's enough bytes for header + something */
	if (rq_arg->len <= RPCRDMA_HDRLEN_ERR)
		goto out_short;

	rdma_argp = rq_arg->head[0].iov_base;
	if (*(rdma_argp + 1) != rpcrdma_version)
		goto out_version;

	switch (*(rdma_argp + 3)) {
	case rdma_msg:
		proc = "RDMA_MSG";
		break;
	case rdma_nomsg:
		proc = "RDMA_NOMSG";
		break;

	case rdma_done:
		goto out_drop;

	case rdma_error:
		goto out_drop;

	default:
		goto out_proc;
	}

	end = (__be32 *)((unsigned long)rdma_argp + rq_arg->len);
	p = xdr_check_read_list(rdma_argp + 4, end);
	if (!p)
		goto out_inval;
	p = xdr_check_write_list(p, end);
	if (!p)
		goto out_inval;
	p = xdr_check_reply_chunk(p, end);
	if (!p)
		goto out_inval;
	if (p > end)
		goto out_inval;

	rq_arg->head[0].iov_base = p;
	hdr_len = (unsigned long)p - (unsigned long)rdma_argp;
	rq_arg->head[0].iov_len -= hdr_len;
	rq_arg->len -= hdr_len;
	dprintk("svcrdma: received %s request for XID 0x%08x, hdr_len=%u\n",
		proc, be32_to_cpup(rdma_argp), hdr_len);
	return hdr_len;

out_short:
	dprintk("svcrdma: header too short = %d\n", rq_arg->len);
	return -EINVAL;

out_version:
	dprintk("svcrdma: bad xprt version: %u\n",
		be32_to_cpup(rdma_argp + 1));
	return -EPROTONOSUPPORT;

out_drop:
	dprintk("svcrdma: dropping RDMA_DONE/ERROR message\n");
	return 0;

out_proc:
	dprintk("svcrdma: bad rdma procedure (%u)\n",
		be32_to_cpup(rdma_argp + 3));
	return -EINVAL;

out_inval:
	dprintk("svcrdma: failed to parse transport header\n");
	return -EINVAL;
}

static void rdma_read_complete(struct svc_rqst *rqstp,
			       struct svc_rdma_op_ctxt *head)
{
	int page_no;

	/* Copy RPC pages */
	for (page_no = 0; page_no < head->count; page_no++) {
		put_page(rqstp->rq_pages[page_no]);
		rqstp->rq_pages[page_no] = head->pages[page_no];
	}

	/* Point rq_arg.pages past header */
	rqstp->rq_arg.pages = &rqstp->rq_pages[head->hdr_count];
	rqstp->rq_arg.page_len = head->arg.page_len;

	/* rq_respages starts after the last arg page */
	rqstp->rq_respages = &rqstp->rq_pages[page_no];
	rqstp->rq_next_page = rqstp->rq_respages + 1;

	/* Rebuild rq_arg head and tail. */
	rqstp->rq_arg.head[0] = head->arg.head[0];
	rqstp->rq_arg.tail[0] = head->arg.tail[0];
	rqstp->rq_arg.len = head->arg.len;
	rqstp->rq_arg.buflen = head->arg.buflen;
}

static void svc_rdma_send_error(struct svcxprt_rdma *xprt,
				__be32 *rdma_argp, int status)
{
	struct svc_rdma_op_ctxt *ctxt;
	__be32 *p, *err_msgp;
	unsigned int length;
	struct page *page;
	int ret;

	ret = svc_rdma_repost_recv(xprt, GFP_KERNEL);
	if (ret)
		return;

	page = alloc_page(GFP_KERNEL);
	if (!page)
		return;
	err_msgp = page_address(page);

	p = err_msgp;
	*p++ = *rdma_argp;
	*p++ = *(rdma_argp + 1);
	*p++ = xprt->sc_fc_credits;
	*p++ = rdma_error;
	if (status == -EPROTONOSUPPORT) {
		*p++ = err_vers;
		*p++ = rpcrdma_version;
		*p++ = rpcrdma_version;
	} else {
		*p++ = err_chunk;
	}
	length = (unsigned long)p - (unsigned long)err_msgp;

	/* Map transport header; no RPC message payload */
	ctxt = svc_rdma_get_context(xprt);
	ret = svc_rdma_map_reply_hdr(xprt, ctxt, err_msgp, length);
	if (ret) {
		dprintk("svcrdma: Error %d mapping send for protocol error\n",
			ret);
		return;
	}

	ret = svc_rdma_post_send_wr(xprt, ctxt, 1, 0);
	if (ret) {
		dprintk("svcrdma: Error %d posting send for protocol error\n",
			ret);
		svc_rdma_unmap_dma(ctxt);
		svc_rdma_put_context(ctxt, 1);
	}
}

/* By convention, backchannel calls arrive via rdma_msg type
 * messages, and never populate the chunk lists. This makes
 * the RPC/RDMA header small and fixed in size, so it is
 * straightforward to check the RPC header's direction field.
 */
static bool svc_rdma_is_backchannel_reply(struct svc_xprt *xprt,
					  __be32 *rdma_resp)
{
	__be32 *p;

	if (!xprt->xpt_bc_xprt)
		return false;

	p = rdma_resp + 3;
	if (*p++ != rdma_msg)
		return false;

	if (*p++ != xdr_zero)
		return false;
	if (*p++ != xdr_zero)
		return false;
	if (*p++ != xdr_zero)
		return false;

	/* XID sanity */
	if (*p++ != *rdma_resp)
		return false;
	/* call direction */
	if (*p == cpu_to_be32(RPC_CALL))
		return false;

	return true;
}

/**
 * svc_rdma_recvfrom - Receive an RPC call
 * @rqstp: request structure into which to receive an RPC Call
 *
 * Returns:
 *	The positive number of bytes in the RPC Call message,
 *	%0 if there were no Calls ready to return,
 *	%-EINVAL if the Read chunk data is too large,
 *	%-ENOMEM if rdma_rw context pool was exhausted,
 *	%-ENOTCONN if posting failed (connection is lost),
 *	%-EIO if rdma_rw initialization failed (DMA mapping, etc).
 *
 * Called in a loop when XPT_DATA is set. XPT_DATA is cleared only
 * when there are no remaining ctxt's to process.
 *
 * The next ctxt is removed from the "receive" lists.
 *
 * - If the ctxt completes a Read, then finish assembling the Call
 *   message and return the number of bytes in the message.
 *
 * - If the ctxt completes a Receive, then construct the Call
 *   message from the contents of the Receive buffer.
 *
 *   - If there are no Read chunks in this message, then finish
 *     assembling the Call message and return the number of bytes
 *     in the message.
 *
 *   - If there are Read chunks in this message, post Read WRs to
 *     pull that payload and return 0.
 */
int svc_rdma_recvfrom(struct svc_rqst *rqstp)
{
	struct svc_xprt *xprt = rqstp->rq_xprt;
	struct svcxprt_rdma *rdma_xprt =
		container_of(xprt, struct svcxprt_rdma, sc_xprt);
	struct svc_rdma_op_ctxt *ctxt;
	__be32 *p;
	int ret;

	spin_lock(&rdma_xprt->sc_rq_dto_lock);
	if (!list_empty(&rdma_xprt->sc_read_complete_q)) {
		ctxt = list_first_entry(&rdma_xprt->sc_read_complete_q,
					struct svc_rdma_op_ctxt, list);
		list_del(&ctxt->list);
		spin_unlock(&rdma_xprt->sc_rq_dto_lock);
		rdma_read_complete(rqstp, ctxt);
		goto complete;
	} else if (!list_empty(&rdma_xprt->sc_rq_dto_q)) {
		ctxt = list_first_entry(&rdma_xprt->sc_rq_dto_q,
					struct svc_rdma_op_ctxt, list);
		list_del(&ctxt->list);
	} else {
		/* No new incoming requests, terminate the loop */
		clear_bit(XPT_DATA, &xprt->xpt_flags);
		spin_unlock(&rdma_xprt->sc_rq_dto_lock);
		return 0;
	}
	spin_unlock(&rdma_xprt->sc_rq_dto_lock);

	dprintk("svcrdma: recvfrom: ctxt=%p on xprt=%p, rqstp=%p\n",
		ctxt, rdma_xprt, rqstp);
	atomic_inc(&rdma_stat_recv);

	/* Build up the XDR from the receive buffers. */
	rdma_build_arg_xdr(rqstp, ctxt, ctxt->byte_len);

	/* Decode the RDMA header. */
	p = (__be32 *)rqstp->rq_arg.head[0].iov_base;
	ret = svc_rdma_xdr_decode_req(&rqstp->rq_arg);
	if (ret < 0)
		goto out_err;
	if (ret == 0)
		goto out_drop;
	rqstp->rq_xprt_hlen = ret;

	if (svc_rdma_is_backchannel_reply(xprt, p)) {
		ret = svc_rdma_handle_bc_reply(xprt->xpt_bc_xprt, p,
					       &rqstp->rq_arg);
		svc_rdma_put_context(ctxt, 0);
		if (ret)
			goto repost;
		return ret;
	}

	p += rpcrdma_fixed_maxsz;
	if (*p != xdr_zero)
		goto out_readchunk;

complete:
	svc_rdma_put_context(ctxt, 0);
	dprintk("svcrdma: recvfrom: xprt=%p, rqstp=%p, rq_arg.len=%u\n",
		rdma_xprt, rqstp, rqstp->rq_arg.len);
	rqstp->rq_prot = IPPROTO_MAX;
	svc_xprt_copy_addrs(rqstp, xprt);
	return rqstp->rq_arg.len;

out_readchunk:
	ret = svc_rdma_recv_read_chunk(rdma_xprt, rqstp, ctxt, p);
	if (ret < 0)
		goto out_postfail;
	return 0;

out_err:
	svc_rdma_send_error(rdma_xprt, p, ret);
	svc_rdma_put_context(ctxt, 0);
	return 0;

out_postfail:
	if (ret == -EINVAL)
		svc_rdma_send_error(rdma_xprt, p, ret);
	svc_rdma_put_context(ctxt, 1);
	return ret;

out_drop:
	svc_rdma_put_context(ctxt, 1);
repost:
	return svc_rdma_repost_recv(rdma_xprt, GFP_KERNEL);
}
