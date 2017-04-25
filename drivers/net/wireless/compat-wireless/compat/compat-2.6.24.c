/*
 * Copyright 2007	Luis R. Rodriguez <mcgrof@winlab.rutgers.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.24.
 */

#include <net/compat.h>
#include <net/arp.h>

/*
 * We simply won't use it though, just declare it for our wrappers and
 * for usage with tons of code that makes mention to it.
 */
struct net init_net;
EXPORT_SYMBOL(init_net);

/* 2.6.22 and 2.6.23 have eth_header_cache_update defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header_cache_update - update cache entry
 * @hh: destination cache entry
 * @dev: network device
 * @haddr: new hardware address
 *
 * Called by Address Resolution module to notify changes in address.
 */
void eth_header_cache_update(struct hh_cache *hh,
                             struct net_device *dev,
                             unsigned char *haddr)
{
	memcpy(((u8 *) hh->hh_data) + HH_DATA_OFF(sizeof(struct ethhdr)),
		haddr, ETH_ALEN);
}
EXPORT_SYMBOL(eth_header_cache_update);

/* 2.6.22 and 2.6.23 have eth_header_cache defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header_cache - fill cache entry from neighbour
 * @neigh: source neighbour
 * @hh: destination cache entry
 * Create an Ethernet header template from the neighbour.
 */
int eth_header_cache(struct neighbour *neigh, struct hh_cache *hh)
{
	__be16 type = hh->hh_type;
	struct ethhdr *eth;
	const struct net_device *dev = neigh->dev;

	eth = (struct ethhdr *)
	    (((u8 *) hh->hh_data) + (HH_DATA_OFF(sizeof(*eth))));

	if (type == htons(ETH_P_802_3))
		return -1;

	eth->h_proto = type;
	memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
	memcpy(eth->h_dest, neigh->ha, ETH_ALEN);
	hh->hh_len = ETH_HLEN;
	return 0;
}
EXPORT_SYMBOL(eth_header_cache);

/* 2.6.22 and 2.6.23 have eth_header() defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_header - create the Ethernet header
 * @skb:	buffer to alter
 * @dev:	source device
 * @type:	Ethernet type field
 * @daddr: destination address (NULL leave destination address)
 * @saddr: source address (NULL use device source address)
 * @len:   packet length (<= skb->len)
 *
 *
 * Set the protocol type. For a packet of type ETH_P_802_3 we put the length
 * in here instead. It is up to the 802.2 layer to carry protocol information.
 */
int eth_header(struct sk_buff *skb, struct net_device *dev, unsigned short type,
	       void *daddr, void *saddr, unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

	if (type != ETH_P_802_3)
		eth->h_proto = htons(type);
	else
		eth->h_proto = htons(len);

	/*
	 *      Set the source hardware address.
	 */

	if (!saddr)
		saddr = dev->dev_addr;
	memcpy(eth->h_source, saddr, dev->addr_len);

	if (daddr) {
		memcpy(eth->h_dest, daddr, dev->addr_len);
		return ETH_HLEN;
	}

	/*
	 *      Anyway, the loopback-device should never use this function...
	 */

	if (dev->flags & (IFF_LOOPBACK | IFF_NOARP)) {
		memset(eth->h_dest, 0, dev->addr_len);
		return ETH_HLEN;
	}

	return -ETH_HLEN;
}

EXPORT_SYMBOL(eth_header);

/* 2.6.22 and 2.6.23 have eth_rebuild_header defined as extern in include/linux/etherdevice.h
 * and actually defined in net/ethernet/eth.c but 2.6.24 exports it. Lets export it here */

/**
 * eth_rebuild_header- rebuild the Ethernet MAC header.
 * @skb: socket buffer to update
 *
 * This is called after an ARP or IPV6 ndisc it's resolution on this
 * sk_buff. We now let protocol (ARP) fill in the other fields.
 *
 * This routine CANNOT use cached dst->neigh!
 * Really, it is used only when dst->neigh is wrong.
 */
int eth_rebuild_header(struct sk_buff *skb)
{
	struct ethhdr *eth = (struct ethhdr *)skb->data;
	struct net_device *dev = skb->dev;

	switch (eth->h_proto) {
#ifdef CONFIG_INET
	case __constant_htons(ETH_P_IP):
		return arp_find(eth->h_dest, skb);
#endif
	default:
		printk(KERN_DEBUG
		       "%s: unable to resolve type %X addresses.\n",
		       dev->name, (int)eth->h_proto);

		memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(eth_rebuild_header);

