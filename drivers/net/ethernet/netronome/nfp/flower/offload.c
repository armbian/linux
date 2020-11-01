/*
 * Copyright (C) 2017 Netronome Systems, Inc.
 *
 * This software is dual licensed under the GNU General License Version 2,
 * June 1991 as shown in the file COPYING in the top-level directory of this
 * source tree or the BSD 2-Clause License provided below.  You have the
 * option to license this software under the complete terms of either license.
 *
 * The BSD 2-Clause License:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      1. Redistributions of source code must retain the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer.
 *
 *      2. Redistributions in binary form must reproduce the above
 *         copyright notice, this list of conditions and the following
 *         disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/skbuff.h>
#include <net/devlink.h>
#include <net/pkt_cls.h>

#include "cmsg.h"
#include "main.h"
#include "../nfpcore/nfp_cpp.h"
#include "../nfpcore/nfp_nsp.h"
#include "../nfp_app.h"
#include "../nfp_main.h"
#include "../nfp_net.h"
#include "../nfp_port.h"

#define NFP_FLOWER_WHITELIST_DISSECTOR \
	(BIT(FLOW_DISSECTOR_KEY_CONTROL) | \
	 BIT(FLOW_DISSECTOR_KEY_BASIC) | \
	 BIT(FLOW_DISSECTOR_KEY_IPV4_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_IPV6_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_PORTS) | \
	 BIT(FLOW_DISSECTOR_KEY_ETH_ADDRS) | \
	 BIT(FLOW_DISSECTOR_KEY_VLAN) | \
	 BIT(FLOW_DISSECTOR_KEY_IP))

static int
nfp_flower_xmit_flow(struct net_device *netdev,
		     struct nfp_fl_payload *nfp_flow, u8 mtype)
{
	u32 meta_len, key_len, mask_len, act_len, tot_len;
	struct nfp_repr *priv = netdev_priv(netdev);
	struct sk_buff *skb;
	unsigned char *msg;

	meta_len =  sizeof(struct nfp_fl_rule_metadata);
	key_len = nfp_flow->meta.key_len;
	mask_len = nfp_flow->meta.mask_len;
	act_len = nfp_flow->meta.act_len;

	tot_len = meta_len + key_len + mask_len + act_len;

	/* Convert to long words as firmware expects
	 * lengths in units of NFP_FL_LW_SIZ.
	 */
	nfp_flow->meta.key_len >>= NFP_FL_LW_SIZ;
	nfp_flow->meta.mask_len >>= NFP_FL_LW_SIZ;
	nfp_flow->meta.act_len >>= NFP_FL_LW_SIZ;

	skb = nfp_flower_cmsg_alloc(priv->app, tot_len, mtype);
	if (!skb)
		return -ENOMEM;

	msg = nfp_flower_cmsg_get_data(skb);
	memcpy(msg, &nfp_flow->meta, meta_len);
	memcpy(&msg[meta_len], nfp_flow->unmasked_data, key_len);
	memcpy(&msg[meta_len + key_len], nfp_flow->mask_data, mask_len);
	memcpy(&msg[meta_len + key_len + mask_len],
	       nfp_flow->action_data, act_len);

	/* Convert back to bytes as software expects
	 * lengths in units of bytes.
	 */
	nfp_flow->meta.key_len <<= NFP_FL_LW_SIZ;
	nfp_flow->meta.mask_len <<= NFP_FL_LW_SIZ;
	nfp_flow->meta.act_len <<= NFP_FL_LW_SIZ;

	nfp_ctrl_tx(priv->app->ctrl, skb);

	return 0;
}

static bool nfp_flower_check_higher_than_mac(struct tc_cls_flower_offload *f)
{
	return dissector_uses_key(f->dissector,
				  FLOW_DISSECTOR_KEY_IPV4_ADDRS) ||
		dissector_uses_key(f->dissector,
				   FLOW_DISSECTOR_KEY_IPV6_ADDRS) ||
		dissector_uses_key(f->dissector,
				   FLOW_DISSECTOR_KEY_PORTS) ||
		dissector_uses_key(f->dissector, FLOW_DISSECTOR_KEY_ICMP);
}

static int
nfp_flower_calculate_key_layers(struct nfp_fl_key_ls *ret_key_ls,
				struct tc_cls_flower_offload *flow)
{
	struct flow_dissector_key_basic *mask_basic = NULL;
	struct flow_dissector_key_basic *key_basic = NULL;
	struct flow_dissector_key_ip *mask_ip = NULL;
	u32 key_layer_two;
	u8 key_layer;
	int key_size;

	if (flow->dissector->used_keys & ~NFP_FLOWER_WHITELIST_DISSECTOR)
		return -EOPNOTSUPP;

	if (dissector_uses_key(flow->dissector,
			       FLOW_DISSECTOR_KEY_ENC_CONTROL)) {
		struct flow_dissector_key_control *mask_enc_ctl =
			skb_flow_dissector_target(flow->dissector,
						  FLOW_DISSECTOR_KEY_ENC_CONTROL,
						  flow->mask);
		/* We are expecting a tunnel. For now we ignore offloading. */
		if (mask_enc_ctl->addr_type)
			return -EOPNOTSUPP;
	}

	if (dissector_uses_key(flow->dissector, FLOW_DISSECTOR_KEY_BASIC)) {
		mask_basic = skb_flow_dissector_target(flow->dissector,
						       FLOW_DISSECTOR_KEY_BASIC,
						       flow->mask);

		key_basic = skb_flow_dissector_target(flow->dissector,
						      FLOW_DISSECTOR_KEY_BASIC,
						      flow->key);
	}

	if (dissector_uses_key(flow->dissector, FLOW_DISSECTOR_KEY_IP))
		mask_ip = skb_flow_dissector_target(flow->dissector,
						    FLOW_DISSECTOR_KEY_IP,
						    flow->mask);

	key_layer_two = 0;
	key_layer = NFP_FLOWER_LAYER_PORT | NFP_FLOWER_LAYER_MAC;
	key_size = sizeof(struct nfp_flower_meta_one) +
		   sizeof(struct nfp_flower_in_port) +
		   sizeof(struct nfp_flower_mac_mpls);

	if (mask_basic && mask_basic->n_proto) {
		/* Ethernet type is present in the key. */
		switch (key_basic->n_proto) {
		case cpu_to_be16(ETH_P_IP):
			if (mask_ip && mask_ip->tos)
				return -EOPNOTSUPP;
			if (mask_ip && mask_ip->ttl)
				return -EOPNOTSUPP;
			key_layer |= NFP_FLOWER_LAYER_IPV4;
			key_size += sizeof(struct nfp_flower_ipv4);
			break;

		case cpu_to_be16(ETH_P_IPV6):
			if (mask_ip && mask_ip->tos)
				return -EOPNOTSUPP;
			if (mask_ip && mask_ip->ttl)
				return -EOPNOTSUPP;
			key_layer |= NFP_FLOWER_LAYER_IPV6;
			key_size += sizeof(struct nfp_flower_ipv6);
			break;

		/* Currently we do not offload ARP
		 * because we rely on it to get to the host.
		 */
		case cpu_to_be16(ETH_P_ARP):
			return -EOPNOTSUPP;

		/* Currently we do not offload MPLS. */
		case cpu_to_be16(ETH_P_MPLS_UC):
		case cpu_to_be16(ETH_P_MPLS_MC):
			return -EOPNOTSUPP;

		/* Will be included in layer 2. */
		case cpu_to_be16(ETH_P_8021Q):
			break;

		default:
			/* Other ethtype - we need check the masks for the
			 * remainder of the key to ensure we can offload.
			 */
			if (nfp_flower_check_higher_than_mac(flow))
				return -EOPNOTSUPP;
			break;
		}
	}

	if (mask_basic && mask_basic->ip_proto) {
		/* Ethernet type is present in the key. */
		switch (key_basic->ip_proto) {
		case IPPROTO_TCP:
		case IPPROTO_UDP:
		case IPPROTO_SCTP:
		case IPPROTO_ICMP:
		case IPPROTO_ICMPV6:
			key_layer |= NFP_FLOWER_LAYER_TP;
			key_size += sizeof(struct nfp_flower_tp_ports);
			break;
		default:
			/* Other ip proto - we need check the masks for the
			 * remainder of the key to ensure we can offload.
			 */
			return -EOPNOTSUPP;
		}
	}

	ret_key_ls->key_layer = key_layer;
	ret_key_ls->key_layer_two = key_layer_two;
	ret_key_ls->key_size = key_size;

	return 0;
}

static struct nfp_fl_payload *
nfp_flower_allocate_new(struct nfp_fl_key_ls *key_layer)
{
	struct nfp_fl_payload *flow_pay;

	flow_pay = kmalloc(sizeof(*flow_pay), GFP_KERNEL);
	if (!flow_pay)
		return NULL;

	flow_pay->meta.key_len = key_layer->key_size;
	flow_pay->unmasked_data = kmalloc(key_layer->key_size, GFP_KERNEL);
	if (!flow_pay->unmasked_data)
		goto err_free_flow;

	flow_pay->meta.mask_len = key_layer->key_size;
	flow_pay->mask_data = kmalloc(key_layer->key_size, GFP_KERNEL);
	if (!flow_pay->mask_data)
		goto err_free_unmasked;

	flow_pay->action_data = kmalloc(NFP_FL_MAX_A_SIZ, GFP_KERNEL);
	if (!flow_pay->action_data)
		goto err_free_mask;

	flow_pay->meta.flags = 0;
	spin_lock_init(&flow_pay->lock);

	return flow_pay;

err_free_mask:
	kfree(flow_pay->mask_data);
err_free_unmasked:
	kfree(flow_pay->unmasked_data);
err_free_flow:
	kfree(flow_pay);
	return NULL;
}

/**
 * nfp_flower_add_offload() - Adds a new flow to hardware.
 * @app:	Pointer to the APP handle
 * @netdev:	netdev structure.
 * @flow:	TC flower classifier offload structure.
 *
 * Adds a new flow to the repeated hash structure and action payload.
 *
 * Return: negative value on error, 0 if configured successfully.
 */
static int
nfp_flower_add_offload(struct nfp_app *app, struct net_device *netdev,
		       struct tc_cls_flower_offload *flow)
{
	struct nfp_flower_priv *priv = app->priv;
	struct nfp_fl_payload *flow_pay;
	struct nfp_fl_key_ls *key_layer;
	int err;

	key_layer = kmalloc(sizeof(*key_layer), GFP_KERNEL);
	if (!key_layer)
		return -ENOMEM;

	err = nfp_flower_calculate_key_layers(key_layer, flow);
	if (err)
		goto err_free_key_ls;

	flow_pay = nfp_flower_allocate_new(key_layer);
	if (!flow_pay) {
		err = -ENOMEM;
		goto err_free_key_ls;
	}

	err = nfp_flower_compile_flow_match(flow, key_layer, netdev, flow_pay);
	if (err)
		goto err_destroy_flow;

	err = nfp_flower_compile_action(flow, netdev, flow_pay);
	if (err)
		goto err_destroy_flow;

	err = nfp_compile_flow_metadata(app, flow, flow_pay);
	if (err)
		goto err_destroy_flow;

	err = nfp_flower_xmit_flow(netdev, flow_pay,
				   NFP_FLOWER_CMSG_TYPE_FLOW_ADD);
	if (err)
		goto err_destroy_flow;

	INIT_HLIST_NODE(&flow_pay->link);
	flow_pay->tc_flower_cookie = flow->cookie;
	hash_add_rcu(priv->flow_table, &flow_pay->link, flow->cookie);

	/* Deallocate flow payload when flower rule has been destroyed. */
	kfree(key_layer);

	return 0;

err_destroy_flow:
	kfree(flow_pay->action_data);
	kfree(flow_pay->mask_data);
	kfree(flow_pay->unmasked_data);
	kfree(flow_pay);
err_free_key_ls:
	kfree(key_layer);
	return err;
}

/**
 * nfp_flower_del_offload() - Removes a flow from hardware.
 * @app:	Pointer to the APP handle
 * @netdev:	netdev structure.
 * @flow:	TC flower classifier offload structure
 *
 * Removes a flow from the repeated hash structure and clears the
 * action payload.
 *
 * Return: negative value on error, 0 if removed successfully.
 */
static int
nfp_flower_del_offload(struct nfp_app *app, struct net_device *netdev,
		       struct tc_cls_flower_offload *flow)
{
	struct nfp_fl_payload *nfp_flow;
	int err;

	nfp_flow = nfp_flower_search_fl_table(app, flow->cookie);
	if (!nfp_flow)
		return -ENOENT;

	err = nfp_modify_flow_metadata(app, nfp_flow);
	if (err)
		goto err_free_flow;

	err = nfp_flower_xmit_flow(netdev, nfp_flow,
				   NFP_FLOWER_CMSG_TYPE_FLOW_DEL);
	if (err)
		goto err_free_flow;

err_free_flow:
	hash_del_rcu(&nfp_flow->link);
	kfree(nfp_flow->action_data);
	kfree(nfp_flow->mask_data);
	kfree(nfp_flow->unmasked_data);
	kfree_rcu(nfp_flow, rcu);
	return err;
}

/**
 * nfp_flower_get_stats() - Populates flow stats obtained from hardware.
 * @app:	Pointer to the APP handle
 * @flow:	TC flower classifier offload structure
 *
 * Populates a flow statistics structure which which corresponds to a
 * specific flow.
 *
 * Return: negative value on error, 0 if stats populated successfully.
 */
static int
nfp_flower_get_stats(struct nfp_app *app, struct tc_cls_flower_offload *flow)
{
	struct nfp_fl_payload *nfp_flow;

	nfp_flow = nfp_flower_search_fl_table(app, flow->cookie);
	if (!nfp_flow)
		return -EINVAL;

	spin_lock_bh(&nfp_flow->lock);
	tcf_exts_stats_update(flow->exts, nfp_flow->stats.bytes,
			      nfp_flow->stats.pkts, nfp_flow->stats.used);

	nfp_flow->stats.pkts = 0;
	nfp_flow->stats.bytes = 0;
	spin_unlock_bh(&nfp_flow->lock);

	return 0;
}

static int
nfp_flower_repr_offload(struct nfp_app *app, struct net_device *netdev,
			struct tc_cls_flower_offload *flower)
{
	switch (flower->command) {
	case TC_CLSFLOWER_REPLACE:
		return nfp_flower_add_offload(app, netdev, flower);
	case TC_CLSFLOWER_DESTROY:
		return nfp_flower_del_offload(app, netdev, flower);
	case TC_CLSFLOWER_STATS:
		return nfp_flower_get_stats(app, flower);
	}

	return -EOPNOTSUPP;
}

int nfp_flower_setup_tc(struct nfp_app *app, struct net_device *netdev,
			enum tc_setup_type type, void *type_data)
{
	struct tc_cls_flower_offload *cls_flower = type_data;

	if (type != TC_SETUP_CLSFLOWER ||
	    !is_classid_clsact_ingress(cls_flower->common.classid) ||
	    !eth_proto_is_802_3(cls_flower->common.protocol) ||
	    cls_flower->common.chain_index)
		return -EOPNOTSUPP;

	return nfp_flower_repr_offload(app, netdev, cls_flower);
}
