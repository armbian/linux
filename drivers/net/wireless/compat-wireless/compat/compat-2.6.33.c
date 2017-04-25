/*
 * Copyright 2009	Hauke Mehrtens <hauke@hauke-m.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Compatibility file for Linux wireless for kernels 2.6.33.
 */

#include <linux/compat.h>
#include <linux/autoconf.h>

#if defined(CONFIG_PCCARD) || defined(CONFIG_PCCARD_MODULE)

/**
 * pccard_loop_tuple() - loop over tuples in the CIS
 * @s:		the struct pcmcia_socket where the card is inserted
 * @function:	the device function we loop for
 * @code:	which CIS code shall we look for?
 * @parse:	buffer where the tuple shall be parsed (or NULL, if no parse)
 * @priv_data:	private data to be passed to the loop_tuple function.
 * @loop_tuple:	function to call for each CIS entry of type @function. IT
 *		gets passed the raw tuple, the paresed tuple (if @parse is
 *		set) and @priv_data.
 *
 * pccard_loop_tuple() loops over all CIS entries of type @function, and
 * calls the @loop_tuple function for each entry. If the call to @loop_tuple
 * returns 0, the loop exits. Returns 0 on success or errorcode otherwise.
 */
int pccard_loop_tuple(struct pcmcia_socket *s, unsigned int function,
		      cisdata_t code, cisparse_t *parse, void *priv_data,
		      int (*loop_tuple) (tuple_t *tuple,
					 cisparse_t *parse,
					 void *priv_data))
{
	tuple_t tuple;
	cisdata_t *buf;
	int ret;

	buf = kzalloc(256, GFP_KERNEL);
	if (buf == NULL) {
		dev_printk(KERN_WARNING, &s->dev, "no memory to read tuple\n");
		return -ENOMEM;
	}

	tuple.TupleData = buf;
	tuple.TupleDataMax = 255;
	tuple.TupleOffset = 0;
	tuple.DesiredTuple = code;
	tuple.Attributes = 0;

	ret = pccard_get_first_tuple(s, function, &tuple);
	while (!ret) {
		if (pccard_get_tuple_data(s, &tuple))
			goto next_entry;

		if (parse)
			if (pcmcia_parse_tuple(&tuple, parse))
				goto next_entry;

		ret = loop_tuple(&tuple, parse, priv_data);
		if (!ret)
			break;

next_entry:
		ret = pccard_get_next_tuple(s, function, &tuple);
	}

	kfree(buf);
	return ret;
}
EXPORT_SYMBOL(pccard_loop_tuple);
/* Source: drivers/pcmcia/cistpl.c */

#if defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)

struct pcmcia_loop_mem {
	struct pcmcia_device *p_dev;
	void *priv_data;
	int (*loop_tuple) (struct pcmcia_device *p_dev,
			   tuple_t *tuple,
			   void *priv_data);
};

/**
 * pcmcia_do_loop_tuple() - internal helper for pcmcia_loop_config()
 *
 * pcmcia_do_loop_tuple() is the internal callback for the call from
 * pcmcia_loop_tuple() to pccard_loop_tuple(). Data is transferred
 * by a struct pcmcia_cfg_mem.
 */
static int pcmcia_do_loop_tuple(tuple_t *tuple, cisparse_t *parse, void *priv)
{
	struct pcmcia_loop_mem *loop = priv;

	return loop->loop_tuple(loop->p_dev, tuple, loop->priv_data);
};

/**
 * pcmcia_loop_tuple() - loop over tuples in the CIS
 * @p_dev:	the struct pcmcia_device which we need to loop for.
 * @code:	which CIS code shall we look for?
 * @priv_data:	private data to be passed to the loop_tuple function.
 * @loop_tuple:	function to call for each CIS entry of type @function. IT
 *		gets passed the raw tuple and @priv_data.
 *
 * pcmcia_loop_tuple() loops over all CIS entries of type @function, and
 * calls the @loop_tuple function for each entry. If the call to @loop_tuple
 * returns 0, the loop exits. Returns 0 on success or errorcode otherwise.
 */
int pcmcia_loop_tuple(struct pcmcia_device *p_dev, cisdata_t code,
		      int (*loop_tuple) (struct pcmcia_device *p_dev,
					 tuple_t *tuple,
					 void *priv_data),
		      void *priv_data)
{
	struct pcmcia_loop_mem loop = {
		.p_dev = p_dev,
		.loop_tuple = loop_tuple,
		.priv_data = priv_data};

	return pccard_loop_tuple(p_dev->socket, p_dev->func, code, NULL,
				 &loop, pcmcia_do_loop_tuple);
}
EXPORT_SYMBOL(pcmcia_loop_tuple);
/* Source: drivers/pcmcia/pcmcia_resource.c */

#endif /* CONFIG_PCMCIA */

#endif /* CONFIG_PCCARD */

