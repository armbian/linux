/*
 * OF helpers for parsing display timings
 *
 * Copyright (c) 2012 Steffen Trumtrar <s.trumtrar@pengutronix.de>, Pengutronix
 *
 * based on of_videomode.c by Sascha Hauer <s.hauer@pengutronix.de>
 *
 * This file is released under the GPLv2
 */
#include <linux/export.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>

/**
 * parse_timing_property - parse timing_entry from device_node
 * @np: device_node with the property
 * @name: name of the property
 * @result: will be set to the return value
 *
 * DESCRIPTION:
 * Every display_timing can be specified with either just the typical value or
 * a range consisting of min/typ/max. This function helps handling this
 **/
static int parse_timing_property(const struct device_node *np, const char *name,
			  struct timing_entry *result)
{
	struct property *prop;
	int length, cells, ret;

	prop = of_find_property(np, name, &length);
	if (!prop) {
		pr_err("%s: could not find property %s\n",
			of_node_full_name(np), name);
		return -EINVAL;
	}

	cells = length / sizeof(u32);
	if (cells == 1) {
		ret = of_property_read_u32(np, name, &result->typ);
		result->min = result->typ;
		result->max = result->typ;
	} else if (cells == 3) {
		ret = of_property_read_u32_array(np, name, &result->min, cells);
	} else {
		pr_err("%s: illegal timing specification in %s\n",
			of_node_full_name(np), name);
		return -EINVAL;
	}

	return ret;
}

/**
 * of_parse_display_timing - parse display_timing entry from device_node
 * @np: device_node with the properties
 **/
static int of_parse_display_timing(const struct device_node *np,
		struct display_timing *dt)
{
	u32 val = 0;
	int ret = 0;
#if defined(CONFIG_FB_ROCKCHIP)
	struct property *prop;
	int length;
#endif

	memset(dt, 0, sizeof(*dt));

	ret |= parse_timing_property(np, "hback-porch", &dt->hback_porch);
	ret |= parse_timing_property(np, "hfront-porch", &dt->hfront_porch);
	ret |= parse_timing_property(np, "hactive", &dt->hactive);
	ret |= parse_timing_property(np, "hsync-len", &dt->hsync_len);
	ret |= parse_timing_property(np, "vback-porch", &dt->vback_porch);
	ret |= parse_timing_property(np, "vfront-porch", &dt->vfront_porch);
	ret |= parse_timing_property(np, "vactive", &dt->vactive);
	ret |= parse_timing_property(np, "vsync-len", &dt->vsync_len);
	ret |= parse_timing_property(np, "clock-frequency", &dt->pixelclock);

	dt->flags = 0;
	if (!of_property_read_u32(np, "vsync-active", &val))
		dt->flags |= val ? DISPLAY_FLAGS_VSYNC_HIGH :
				DISPLAY_FLAGS_VSYNC_LOW;
	if (!of_property_read_u32(np, "hsync-active", &val))
		dt->flags |= val ? DISPLAY_FLAGS_HSYNC_HIGH :
				DISPLAY_FLAGS_HSYNC_LOW;
	if (!of_property_read_u32(np, "de-active", &val))
		dt->flags |= val ? DISPLAY_FLAGS_DE_HIGH :
				DISPLAY_FLAGS_DE_LOW;
	if (!of_property_read_u32(np, "pixelclk-active", &val))
		dt->flags |= val ? DISPLAY_FLAGS_PIXDATA_POSEDGE :
				DISPLAY_FLAGS_PIXDATA_NEGEDGE;
	if (!of_property_read_u32(np, "screen-rotate", &val)) {
		if (val == DRM_MODE_FLAG_XMIRROR) {
			dt->flags |= DISPLAY_FLAGS_MIRROR_X;
		} else if (val ==  DRM_MODE_FLAG_YMIRROR) {
			dt->flags |= DISPLAY_FLAGS_MIRROR_Y;
		} else if (val == DRM_MODE_FLAG_XYMIRROR) {
			dt->flags |= DISPLAY_FLAGS_MIRROR_X;
			dt->flags |= DISPLAY_FLAGS_MIRROR_Y;
		}
	}

	if (of_property_read_bool(np, "interlaced"))
		dt->flags |= DISPLAY_FLAGS_INTERLACED;
	if (of_property_read_bool(np, "doublescan"))
		dt->flags |= DISPLAY_FLAGS_DOUBLESCAN;
	if (of_property_read_bool(np, "doubleclk"))
		dt->flags |= DISPLAY_FLAGS_DOUBLECLK;
#if defined(CONFIG_FB_ROCKCHIP)
	if (!of_property_read_u32(np, "swap-rg", &val))
		dt->flags |= val ? DISPLAY_FLAGS_SWAP_RG : 0;
	if (!of_property_read_u32(np, "swap-gb", &val))
		dt->flags |= val ? DISPLAY_FLAGS_SWAP_GB : 0;
	if (!of_property_read_u32(np, "swap-rb", &val))
		dt->flags |= val ? DISPLAY_FLAGS_SWAP_RB : 0;
	if (!of_property_read_u32(np, "screen-type", &val))
		dt->screen_type = val;
	if (!of_property_read_u32(np, "refresh-mode", &val))
		dt->refresh_mode = val;
	else
		dt->refresh_mode = 0;
	if (!of_property_read_u32(np, "lvds-format", &val))
		dt->lvds_format = val;
	if (!of_property_read_u32(np, "out-face", &val))
		dt->face = val;
	if (!of_property_read_u32(np, "color-mode", &val))
                dt->color_mode = val;
	if (!of_property_read_u32(np, "screen-width", &val))
                dt->screen_widt = val;
	if (!of_property_read_u32(np, "screen-hight", &val))
                dt->screen_hight = val;
	prop = of_find_property(np, "dsp-lut", &length);
	if (prop) {
		dt->dsp_lut = kzalloc(length, GFP_KERNEL);
		if (dt->dsp_lut)
			ret = of_property_read_u32_array(np,
				"dsp-lut", dt->dsp_lut, length >> 2);
	}
	prop = of_find_property(np, "cabc-lut", &length);
	if (prop) {
		dt->cabc_lut = kzalloc(length, GFP_KERNEL);
		if (dt->cabc_lut)
			ret = of_property_read_u32_array(np,
							 "cabc-lut",
							 dt->cabc_lut,
							 length >> 2);
	}

	prop = of_find_property(np, "cabc-gamma-base", &length);
	if (prop) {
		dt->cabc_gamma_base = kzalloc(length, GFP_KERNEL);
		if (dt->cabc_gamma_base)
			ret = of_property_read_u32_array(np,
							 "cabc-gamma-base",
							 dt->cabc_gamma_base,
							 length >> 2);
	}
#endif

	if (ret) {
		pr_err("%s: error reading timing properties\n",
			of_node_full_name(np));
		return -EINVAL;
	}

	return 0;
}

/**
 * of_get_display_timing - parse a display_timing entry
 * @np: device_node with the timing subnode
 * @name: name of the timing node
 * @dt: display_timing struct to fill
 **/
int of_get_display_timing(struct device_node *np, const char *name,
		struct display_timing *dt)
{
	struct device_node *timing_np;

	if (!np)
		return -EINVAL;

	timing_np = of_get_child_by_name(np, name);
	if (!timing_np) {
		pr_err("%s: could not find node '%s'\n",
			of_node_full_name(np), name);
		return -ENOENT;
	}

	return of_parse_display_timing(timing_np, dt);
}
EXPORT_SYMBOL_GPL(of_get_display_timing);

/**
 * of_get_display_timings - parse all display_timing entries from a device_node
 * @np: device_node with the subnodes
 **/
struct display_timings *of_get_display_timings(struct device_node *np)
{
	struct device_node *timings_np;
	struct device_node *entry;
	struct device_node *native_mode;
	struct display_timings *disp;

	if (!np)
		return NULL;

	timings_np = of_get_child_by_name(np, "display-timings");
	if (!timings_np) {
		pr_err("%s: could not find display-timings node\n",
			of_node_full_name(np));
		return NULL;
	}

	disp = kzalloc(sizeof(*disp), GFP_KERNEL);
	if (!disp) {
		pr_err("%s: could not allocate struct disp'\n",
			of_node_full_name(np));
		goto dispfail;
	}

	entry = of_parse_phandle(timings_np, "native-mode", 0);
	/* assume first child as native mode if none provided */
	if (!entry)
		entry = of_get_next_child(timings_np, NULL);
	/* if there is no child, it is useless to go on */
	if (!entry) {
		pr_err("%s: no timing specifications given\n",
			of_node_full_name(np));
		goto entryfail;
	}

	pr_debug("%s: using %s as default timing\n",
		of_node_full_name(np), entry->name);

	native_mode = entry;

	disp->num_timings = of_get_child_count(timings_np);
	if (disp->num_timings == 0) {
		/* should never happen, as entry was already found above */
		pr_err("%s: no timings specified\n", of_node_full_name(np));
		goto entryfail;
	}

	disp->timings = kzalloc(sizeof(struct display_timing *) *
				disp->num_timings, GFP_KERNEL);
	if (!disp->timings) {
		pr_err("%s: could not allocate timings array\n",
			of_node_full_name(np));
		goto entryfail;
	}

	disp->num_timings = 0;
	disp->native_mode = 0;

	for_each_child_of_node(timings_np, entry) {
		struct display_timing *dt;
		int r;

		dt = kzalloc(sizeof(*dt), GFP_KERNEL);
		if (!dt) {
			pr_err("%s: could not allocate display_timing struct\n",
					of_node_full_name(np));
			goto timingfail;
		}

		r = of_parse_display_timing(entry, dt);
		if (r) {
			/*
			 * to not encourage wrong devicetrees, fail in case of
			 * an error
			 */
			pr_err("%s: error in timing %d\n",
				of_node_full_name(np), disp->num_timings + 1);
			kfree(dt);
			goto timingfail;
		}

		if (native_mode == entry)
			disp->native_mode = disp->num_timings;

		disp->timings[disp->num_timings] = dt;
		disp->num_timings++;
	}
	of_node_put(timings_np);
	/*
	 * native_mode points to the device_node returned by of_parse_phandle
	 * therefore call of_node_put on it
	 */
	of_node_put(native_mode);

	pr_debug("%s: got %d timings. Using timing #%d as default\n",
		of_node_full_name(np), disp->num_timings,
		disp->native_mode + 1);

	return disp;

timingfail:
	of_node_put(native_mode);
	display_timings_release(disp);
	disp = NULL;
entryfail:
	kfree(disp);
dispfail:
	of_node_put(timings_np);
	return NULL;
}
EXPORT_SYMBOL_GPL(of_get_display_timings);

/**
 * of_display_timings_exist - check if a display-timings node is provided
 * @np: device_node with the timing
 **/
int of_display_timings_exist(struct device_node *np)
{
	struct device_node *timings_np;

	if (!np)
		return -EINVAL;

	timings_np = of_parse_phandle(np, "display-timings", 0);
	if (!timings_np)
		return -EINVAL;

	of_node_put(timings_np);
	return 1;
}
EXPORT_SYMBOL_GPL(of_display_timings_exist);
