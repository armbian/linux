// SPDX-License-Identifier: GPL-2.0
/**
 * Rockchip rk1608 driver
 *
 * Copyright (C) 2017-2018 Rockchip Electronics Co., Ltd.
 *
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/regmap.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/mfd/syscon.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/rk-preisp.h>
#include <linux/rkisp1-config.h>
#include <linux/rk-camera-module.h>
#include "rk1608_dphy.h"

#define RK1608_DPHY_NAME	"RK1608-dphy"

/**
 * Rk1608 is used as the Pre-ISP to link on Soc, which mainly has two
 * functions. One is to download the firmware of RK1608, and the other
 * is to match the extra sensor such as camera and enable sensor by
 * calling sensor's s_power.
 *	|-----------------------|
 *	|     Sensor Camera     |
 *	|-----------------------|
 *	|-----------||----------|
 *	|-----------||----------|
 *	|-----------\/----------|
 *	|     Pre-ISP RK1608    |
 *	|-----------------------|
 *	|-----------||----------|
 *	|-----------||----------|
 *	|-----------\/----------|
 *	|      Rockchip Soc     |
 *	|-----------------------|
 * Data Transfer As shown above. In RK1608, the data received from the
 * extra sensor,and it is passed to the Soc through ISP.
 */

static inline struct rk1608_dphy *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct rk1608_dphy, sd);
}

static int rk1608_s_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_ctrl *remote_ctrl;
	struct rk1608_dphy *pdata = to_state(sd);

	pdata->rk1608_sd->grp_id = pdata->sd.grp_id;
	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
				     V4L2_CID_HBLANK);
	if (remote_ctrl) {
		v4l2_ctrl_g_ctrl(remote_ctrl);
		__v4l2_ctrl_modify_range(pdata->hblank,
					 remote_ctrl->minimum,
					 remote_ctrl->maximum,
					 remote_ctrl->step,
					 remote_ctrl->default_value);
	}

	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
				     V4L2_CID_VBLANK);
	if (remote_ctrl) {
		v4l2_ctrl_g_ctrl(remote_ctrl);
		__v4l2_ctrl_modify_range(pdata->vblank,
					 remote_ctrl->minimum,
					 remote_ctrl->maximum,
					 remote_ctrl->step,
					 remote_ctrl->default_value);
	}

	return 0;
}

static int rk1608_s_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct rk1608_dphy *pdata = to_state(sd);

	pdata->rk1608_sd->grp_id = sd->grp_id;

	return 0;
}

static int rk1608_sensor_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;
	struct rk1608_dphy *pdata = to_state(sd);

	pdata->rk1608_sd->grp_id = sd->grp_id;
	ret = v4l2_subdev_call(pdata->rk1608_sd, core, s_power, on);

	return ret;
}

static int rk1608_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct rk1608_dphy *pdata = to_state(sd);

	pdata->rk1608_sd->grp_id = sd->grp_id;
	v4l2_subdev_call(pdata->rk1608_sd, video, s_stream, enable);
	return 0;
}

static int rk1608_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct rk1608_dphy *pdata = to_state(sd);

	if (code->index >= pdata->fmt_inf_num)
		return -EINVAL;

	code->code = pdata->fmt_inf[code->index].mf.code;

	return 0;
}

static int rk1608_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct rk1608_dphy *pdata = to_state(sd);

	if (fse->index >= pdata->fmt_inf_num)
		return -EINVAL;

	if (fse->code != pdata->fmt_inf[fse->index].mf.code)
		return -EINVAL;

	fse->min_width  = pdata->fmt_inf[fse->index].mf.width;
	fse->max_width  = pdata->fmt_inf[fse->index].mf.width;
	fse->max_height = pdata->fmt_inf[fse->index].mf.height;
	fse->min_height = pdata->fmt_inf[fse->index].mf.height;

	return 0;
}

static int rk1608_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	struct rk1608_dphy *pdata = to_state(sd);
	u32 idx = pdata->fmt_inf_idx;

	mf->code = pdata->fmt_inf[idx].mf.code;
	mf->width = pdata->fmt_inf[idx].mf.width;
	mf->height = pdata->fmt_inf[idx].mf.height;
	mf->field = pdata->fmt_inf[idx].mf.field;
	mf->colorspace = pdata->fmt_inf[idx].mf.colorspace;

	return 0;
}

static int rk1608_get_reso_dist(struct rk1608_fmt_inf *fmt_inf,
				struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *framefmt = &fmt->format;

	return abs(fmt_inf->mf.width - framefmt->width) +
	       abs(fmt_inf->mf.height - framefmt->height);
}

static int rk1608_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct v4l2_ctrl *remote_ctrl;
	struct rk1608_dphy *pdata = to_state(sd);
	u32 i, idx = 0;
	int dist;
	int cur_best_fit_dist = -1;

	for (i = 0; i < pdata->fmt_inf_num; i++) {
		dist = rk1608_get_reso_dist(&pdata->fmt_inf[i], fmt);
		if (cur_best_fit_dist == -1 || dist < cur_best_fit_dist) {
			cur_best_fit_dist = dist;
			idx = i;
		}
	}

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY)
		return -ENOTTY;

	pdata->fmt_inf_idx = idx;

	v4l2_subdev_call(pdata->rk1608_sd, pad, set_fmt, cfg, fmt);

	pdata->rk1608_sd->grp_id = pdata->sd.grp_id;
	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
						 V4L2_CID_HBLANK);
	if (remote_ctrl) {
		v4l2_ctrl_g_ctrl(remote_ctrl);
		__v4l2_ctrl_modify_range(pdata->hblank,
					 remote_ctrl->minimum,
					 remote_ctrl->maximum,
					 remote_ctrl->step,
					 remote_ctrl->default_value);
	}

	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
					 V4L2_CID_VBLANK);
	if (remote_ctrl) {
		v4l2_ctrl_g_ctrl(remote_ctrl);
		__v4l2_ctrl_modify_range(pdata->vblank,
					 remote_ctrl->minimum,
					 remote_ctrl->maximum,
					 remote_ctrl->step,
					 remote_ctrl->default_value);
	}

	return 0;
}

static int rk1608_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct rk1608_dphy *pdata = to_state(sd);

	pdata->rk1608_sd->grp_id = sd->grp_id;
	v4l2_subdev_call(pdata->rk1608_sd,
			 video,
			 g_frame_interval,
			 fi);

	return 0;
}

static int rk1608_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	return 0;
}

static long rk1608_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct rk1608_dphy *pdata = to_state(sd);
	long ret = 0;

	switch (cmd) {
	case PREISP_CMD_SAVE_HDRAE_PARAM:
	case PREISP_CMD_SET_HDRAE_EXP:
	case RKMODULE_GET_MODULE_INFO:
		pdata->rk1608_sd->grp_id = pdata->sd.grp_id;
		ret = v4l2_subdev_call(pdata->rk1608_sd, core, ioctl,
				       cmd, arg);
		return ret;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long rk1608_compat_ioctl32(struct v4l2_subdev *sd,
		     unsigned int cmd, unsigned long arg)
{
	void __user *up = compat_ptr(arg);
	struct preisp_hdrae_exp_s hdrae_exp;
	struct rkmodule_inf *inf;
	long ret;

	switch (cmd) {
	case PREISP_CMD_SET_HDRAE_EXP:
		if (copy_from_user(&hdrae_exp, up, sizeof(hdrae_exp)))
			return -EFAULT;

		return rk1608_ioctl(sd, cmd, &hdrae_exp);
	case RKMODULE_GET_MODULE_INFO:
		inf = kzalloc(sizeof(*inf), GFP_KERNEL);
		if (!inf) {
			ret = -ENOMEM;
			return ret;
		}

		ret = rk1608_ioctl(sd, cmd, inf);
		if (!ret)
			ret = copy_to_user(up, inf, sizeof(*inf));
		kfree(inf);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}
#endif

static int rk1608_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *remote_ctrl;
	struct rk1608_dphy *pdata =
		container_of(ctrl->handler,
			     struct rk1608_dphy, ctrl_handler);

	pdata->rk1608_sd->grp_id = pdata->sd.grp_id;
	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
				     ctrl->id);
	if (remote_ctrl) {
		ctrl->val = v4l2_ctrl_g_ctrl(remote_ctrl);
		__v4l2_ctrl_modify_range(ctrl,
					 remote_ctrl->minimum,
					 remote_ctrl->maximum,
					 remote_ctrl->step,
					 remote_ctrl->default_value);
	}

	return 0;
}

static int rk1608_set_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	struct v4l2_ctrl *remote_ctrl;
	struct rk1608_dphy *pdata =
		container_of(ctrl->handler,
			     struct rk1608_dphy, ctrl_handler);

	pdata->rk1608_sd->grp_id = pdata->sd.grp_id;
	remote_ctrl = v4l2_ctrl_find(pdata->rk1608_sd->ctrl_handler,
				     ctrl->id);
	if (remote_ctrl)
		ret = v4l2_ctrl_s_ctrl(remote_ctrl, ctrl->val);

	return ret;
}

static const struct v4l2_ctrl_ops rk1608_ctrl_ops = {
	.g_volatile_ctrl = rk1608_g_volatile_ctrl,
	.s_ctrl = rk1608_set_ctrl,
};

static const struct v4l2_ctrl_config rk1608_priv_ctrls[] = {
	{
		.ops	= NULL,
		.id	= CIFISP_CID_EMB_VC,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Embedded visual channel",
		.min	= 0,
		.max	= 3,
		.def	= 0,
		.step	= 1,
	}, {
		.ops	= NULL,
		.id	= CIFISP_CID_EMB_DT,
		.type	= V4L2_CTRL_TYPE_INTEGER,
		.name	= "Embedded data type",
		.min	= 0,
		.max	= 0xff,
		.def	= 0x30,
		.step	= 1,
	}
};

static int rk1608_initialize_controls(struct rk1608_dphy *dphy)
{
	u32 i;
	int ret;
	s64 pixel_rate, pixel_bit;
	u32 idx = dphy->fmt_inf_idx;
	struct v4l2_ctrl_handler *handler;
	unsigned long flags = V4L2_CTRL_FLAG_VOLATILE |
			      V4L2_CTRL_FLAG_EXECUTE_ON_WRITE;

	handler = &dphy->ctrl_handler;
	ret = v4l2_ctrl_handler_init(handler, 8);
	if (ret)
		return ret;

	dphy->link_freq = v4l2_ctrl_new_int_menu(handler, NULL,
				       V4L2_CID_LINK_FREQ, 0,
				       0, &dphy->link_freqs);
	if (dphy->link_freq)
		dphy->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	switch (dphy->fmt_inf[idx].data_type) {
	case 0x2b:
		pixel_bit = 10;
		break;
	case 0x2c:
		pixel_bit = 12;
		break;
	default:
		pixel_bit = 8;
		break;
	}
	pixel_rate = dphy->link_freqs * dphy->fmt_inf[idx].mipi_lane * 2;
	do_div(pixel_rate, pixel_bit);
	dphy->pixel_rate = v4l2_ctrl_new_std(handler, NULL,
					     V4L2_CID_PIXEL_RATE,
					     0, pixel_rate, 1, pixel_rate);

	dphy->hblank = v4l2_ctrl_new_std(handler,
					 &rk1608_ctrl_ops,
					 V4L2_CID_HBLANK,
					 0, 0x7FFFFFFF, 1, 0);
	if (dphy->hblank)
		dphy->hblank->flags |= flags;

	dphy->vblank = v4l2_ctrl_new_std(handler,
					 &rk1608_ctrl_ops,
					 V4L2_CID_VBLANK,
					 0, 0x7FFFFFFF, 1, 0);
	if (dphy->vblank)
		dphy->vblank->flags |= flags;

	dphy->exposure = v4l2_ctrl_new_std(handler,
					   &rk1608_ctrl_ops,
					   V4L2_CID_EXPOSURE,
					   0, 0x7FFFFFFF, 1, 0);
	if (dphy->exposure)
		dphy->exposure->flags |= flags;

	dphy->gain = v4l2_ctrl_new_std(handler,
				       &rk1608_ctrl_ops,
				       V4L2_CID_ANALOGUE_GAIN,
				       0, 0x7FFFFFFF, 1, 0);
	if (dphy->gain)
		dphy->gain->flags |= flags;

	for (i = 0; i < ARRAY_SIZE(rk1608_priv_ctrls); i++)
		v4l2_ctrl_new_custom(handler, &rk1608_priv_ctrls[i], NULL);

	if (handler->error) {
		ret = handler->error;
		dev_err(dphy->dev,
			"Failed to init controls(%d)\n", ret);
		goto err_free_handler;
	}

	dphy->sd.ctrl_handler = handler;

	return 0;

err_free_handler:
	v4l2_ctrl_handler_free(handler);

	return ret;
}

static const struct v4l2_subdev_internal_ops dphy_subdev_internal_ops = {
	.open	= rk1608_s_open,
	.close	= rk1608_s_close,
};

static const struct v4l2_subdev_video_ops rk1608_subdev_video_ops = {
	.s_stream	= rk1608_s_stream,
	.g_frame_interval = rk1608_g_frame_interval,
	.s_frame_interval = rk1608_s_frame_interval,
};

static const struct v4l2_subdev_pad_ops rk1608_subdev_pad_ops = {
	.enum_mbus_code	= rk1608_enum_mbus_code,
	.enum_frame_size = rk1608_enum_frame_sizes,
	.get_fmt	= rk1608_get_fmt,
	.set_fmt	= rk1608_set_fmt,
};

static const struct v4l2_subdev_core_ops rk1608_core_ops = {
	.s_power	= rk1608_sensor_power,
	.ioctl		= rk1608_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = rk1608_compat_ioctl32,
#endif
};

static const struct v4l2_subdev_ops dphy_subdev_ops = {
	.core	= &rk1608_core_ops,
	.video	= &rk1608_subdev_video_ops,
	.pad	= &rk1608_subdev_pad_ops,
};

static int rk1608_dphy_dt_property(struct rk1608_dphy *dphy)
{
	int ret = 0;
	struct device_node *node = dphy->dev->of_node;
	struct device_node *parent_node = of_node_get(node);
	struct device_node *prev_node = NULL;
	u32 idx = 0;

	ret = of_property_read_u32(node, "id", &dphy->sd.grp_id);
	if (ret)
		dev_warn(dphy->dev, "Can not get id!");

	ret = of_property_read_u32(node, "cam_nums", &dphy->cam_nums);
	if (ret)
		dev_warn(dphy->dev, "Can not get cam_nums!");

	ret = of_property_read_u32(node, "in_mipi", &dphy->in_mipi);
	if (ret)
		dev_warn(dphy->dev, "Can not get in_mipi!");

	ret = of_property_read_u32(node, "out_mipi", &dphy->out_mipi);
	if (ret)
		dev_warn(dphy->dev, "Can not get out_mipi!");

	ret = of_property_read_u64(node, "link-freqs", &dphy->link_freqs);
	if (ret)
		dev_warn(dphy->dev, "Can not get link_freqs!");

	ret = of_property_read_u32(node, "sensor_i2c_bus", &dphy->i2c_bus);
	if (ret)
		dev_warn(dphy->dev, "Can not get sensor_i2c_bus!");

	ret = of_property_read_u32(node, "sensor_i2c_addr", &dphy->i2c_addr);
	if (ret)
		dev_warn(dphy->dev, "Can not get sensor_i2c_addr!");

	ret = of_property_read_string(node, "sensor-name", &dphy->sensor_name);
	if (ret)
		dev_warn(dphy->dev, "Can not get sensor-name!");

	node = NULL;
	while (!IS_ERR_OR_NULL(node =
				of_get_next_child(parent_node, prev_node))) {
		if (!strncasecmp(node->name,
				 "format-config",
				 strlen("format-config"))) {
			ret = of_property_read_u32(node, "data_type",
				&dphy->fmt_inf[idx].data_type);
			if (ret)
				dev_warn(dphy->dev, "Can not get data_type!");

			ret = of_property_read_u32(node, "mipi_lane",
				&dphy->fmt_inf[idx].mipi_lane);
			if (ret)
				dev_warn(dphy->dev, "Can not get mipi_lane!");

			ret = of_property_read_u32(node, "field",
				&dphy->fmt_inf[idx].mf.field);
			if (ret)
				dev_warn(dphy->dev, "Can not get field!");

			ret = of_property_read_u32(node, "colorspace",
				&dphy->fmt_inf[idx].mf.colorspace);
			if (ret)
				dev_warn(dphy->dev, "Can not get colorspace!");

			ret = of_property_read_u32(node, "code",
				&dphy->fmt_inf[idx].mf.code);
			if (ret)
				dev_warn(dphy->dev, "Can not get code!");

			ret = of_property_read_u32(node, "width",
				&dphy->fmt_inf[idx].mf.width);
			if (ret)
				dev_warn(dphy->dev, "Can not get width!");

			ret = of_property_read_u32(node, "height",
				&dphy->fmt_inf[idx].mf.height);
			if (ret)
				dev_warn(dphy->dev, "Can not get height!");

			ret = of_property_read_u32(node, "hactive",
				&dphy->fmt_inf[idx].hactive);
			if (ret)
				dev_warn(dphy->dev, "Can not get hactive!");

			ret = of_property_read_u32(node, "vactive",
				&dphy->fmt_inf[idx].vactive);
			if (ret)
				dev_warn(dphy->dev, "Can not get vactive!");

			ret = of_property_read_u32(node, "htotal",
				&dphy->fmt_inf[idx].htotal);
			if (ret)
				dev_warn(dphy->dev, "Can not get htotal!");

			ret = of_property_read_u32(node, "vtotal",
				&dphy->fmt_inf[idx].vtotal);
			if (ret)
				dev_warn(dphy->dev, "Can not get vtotal!");

			ret = of_property_read_u32_array(node, "inch0-info",
				(u32 *)&dphy->fmt_inf[idx].in_ch[0], 5);
			if (ret)
				dev_warn(dphy->dev, "Can not get inch0-info!");

			ret = of_property_read_u32_array(node, "inch1-info",
				(u32 *)&dphy->fmt_inf[idx].in_ch[1], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get inch1-info!");

			ret = of_property_read_u32_array(node, "inch2-info",
				(u32 *)&dphy->fmt_inf[idx].in_ch[2], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get inch2-info!");

			ret = of_property_read_u32_array(node, "inch3-info",
				(u32 *)&dphy->fmt_inf[idx].in_ch[3], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get inch3-info!");

			ret = of_property_read_u32_array(node, "outch0-info",
				(u32 *)&dphy->fmt_inf[idx].out_ch[0], 5);
			if (ret)
				dev_warn(dphy->dev, "Can not get outch0-info!");

			ret = of_property_read_u32_array(node, "outch1-info",
				(u32 *)&dphy->fmt_inf[idx].out_ch[1], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get outch1-info!");

			ret = of_property_read_u32_array(node, "outch2-info",
				(u32 *)&dphy->fmt_inf[idx].out_ch[2], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get outch2-info!");

			ret = of_property_read_u32_array(node, "outch3-info",
				(u32 *)&dphy->fmt_inf[idx].out_ch[3], 5);
			if (ret)
				dev_info(dphy->dev, "Can not get outch3-info!");

			idx++;
		}

		of_node_put(prev_node);
		prev_node = node;
	}
	dphy->fmt_inf_num = idx;

	of_node_put(prev_node);
	of_node_put(parent_node);

	return ret;
}

static int rk1608_dphy_probe(struct platform_device *pdev)
{
	struct rk1608_dphy *dphy;
	struct v4l2_subdev *sd;
	struct device_node *node = pdev->dev.of_node;
	char facing[2];
	int ret = 0;

	dphy = devm_kzalloc(&pdev->dev, sizeof(*dphy), GFP_KERNEL);
	if (!dphy)
		return -ENOMEM;

	ret = of_property_read_u32(node, RKMODULE_CAMERA_MODULE_INDEX,
				   &dphy->module_index);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_FACING,
				       &dphy->module_facing);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_MODULE_NAME,
				       &dphy->module_name);
	ret |= of_property_read_string(node, RKMODULE_CAMERA_LENS_NAME,
				       &dphy->len_name);
	if (ret) {
		dev_err(dphy->dev,
			"could not get module information!\n");
		return -EINVAL;
	}

	dphy->dev = &pdev->dev;
	platform_set_drvdata(pdev, dphy);
	sd = &dphy->sd;
	sd->dev = &pdev->dev;
	v4l2_subdev_init(sd, &dphy_subdev_ops);
	rk1608_dphy_dt_property(dphy);

	memset(facing, 0, sizeof(facing));
	if (strcmp(dphy->module_facing, "back") == 0)
		facing[0] = 'b';
	else
		facing[0] = 'f';

	snprintf(sd->name, sizeof(sd->name), "m%02d_%s_%s RK1608-dphy%d",
		 dphy->module_index, facing,
		 RK1608_DPHY_NAME, sd->grp_id);
	rk1608_initialize_controls(dphy);
	sd->internal_ops = &dphy_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dphy->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&sd->entity, 1, &dphy->pad, 0);
	if (ret < 0)
		goto handler_err;
	ret = v4l2_async_register_subdev_sensor_common(sd);
	if (ret < 0)
		goto register_err;

	dev_info(dphy->dev, "RK1608-dphy(%d) probe success!\n", sd->grp_id);

	return 0;
register_err:
	media_entity_cleanup(&sd->entity);
handler_err:
	v4l2_ctrl_handler_free(dphy->sd.ctrl_handler);
	devm_kfree(&pdev->dev, dphy);
	return ret;
}

static int rk1608_dphy_remove(struct platform_device *pdev)
{
	struct rk1608_dphy *dphy = platform_get_drvdata(pdev);

	v4l2_async_unregister_subdev(&dphy->sd);
	media_entity_cleanup(&dphy->sd.entity);
	v4l2_ctrl_handler_free(&dphy->ctrl_handler);

	return 0;
}

static const struct of_device_id dphy_of_match[] = {
	{ .compatible = "rockchip,rk1608-dphy" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, rk1608_of_match);

static struct platform_driver rk1608_dphy_drv = {
	.driver = {
		.of_match_table = of_match_ptr(dphy_of_match),
		.name	= RK1608_DPHY_NAME,
	},
	.probe		= rk1608_dphy_probe,
	.remove		= rk1608_dphy_remove,
};

module_platform_driver(rk1608_dphy_drv);

MODULE_AUTHOR("Rockchip Camera/ISP team");
MODULE_DESCRIPTION("A DSP driver for rk1608 chip");
MODULE_LICENSE("GPL v2");
