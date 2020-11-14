/*
 * rk610.c -- RK610 ALSA SoC audio driver
 *
 * Copyright (C) 2009 rockchip lhh
 *
 *
 * Based on RK610.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include "rk610_codec.h"

#define RK610_PROC

#define HP_OUT 0
#define HP_IN  1

//if you find resume rk610 cannot work,you can try RESUME_PROBLEM 1.
//if rk610 Normal working on RESUME_PROBLEM 1, you must detect Machine other driver queue.
//you can look soc-core.c the resume source.s
#define RESUME_PROBLEM 0

//1:set pll from rk610
#define RK610_CTL_PLL 0

/*
 * Debug
 */
#if 0
#define	DBG(x...)	printk(KERN_INFO x)
#else
#define	DBG(x...)
#endif

//rk610 output volume,DAC Digital Gain
//0x0000 ~ 0xF42					
#define Volume_Output 0xF42
//0x0 ~ 0x3f(bit0-bit5)	 max=0x0(+6DB) min=0x3f(-60DB)	//Analog Gain
#define Volume_Codec_PA 0x0

//rk610 input volume,rk610 can not adjust the recording volume 
#define Volume_Input 0x07



#define OUT_CAPLESS  (1)   //�Ƿ�Ϊ�޵��������1:�޵��������0:�е������

static u8 gR0AReg = 0;  //���ڼ�¼R0A�Ĵ�����ֵ�����ڸı������ǰͨ��R0Aֹͣclk
static u8 gR0BReg = 0;  //���ڼ�¼R0B�Ĵ�����ֵ�����ڸı������ǰͨ��R0Bֹͣinterplate��decimation


/*
 * rk610 register cache
 * We can't read the RK610 register space when we
 * are using 2 wire for device control, so we cache them instead.
 */
static const u16 rk610_codec_reg[] = {
	0x0005, 0x0004, 0x00fd, 0x00f3,  /*  0 */
	0x0003, 0x0000, 0x0000, 0x0000,  /*  4 */
	0x0000, 0x0005, 0x0000, 0x0000,  /*  8 */
	0x0097, 0x0097, 0x0097, 0x0097,  /* 12 */
	0x0097, 0x0097, 0x00cc, 0x0000,  /* 16 */
	0x0000, 0x00f1, 0x0090, 0x00ff,  /* 20 */
	0x00ff, 0x00ff, 0x009c, 0x0000,  /* 24 */
	0x0000, 0x00ff, 0x00ff, 0x00ff,  /* 28 */
};

static struct snd_soc_codec *rk610_codec_codec=NULL;
/* codec private data */
struct rk610_codec_priv {
	enum snd_soc_control_type control_type;
	unsigned int sysclk;
	struct snd_soc_codec codec;
	struct snd_pcm_hw_constraint_list *sysclk_constraints;
	u16 reg_cache[RK610_CODEC_NUM_REG];

	struct delayed_work rk610_delayed_work;
	unsigned int spk_ctrl_io;
	/*
		Some amplifiers enable a longer time.
		config after pa_enable_io delay pa_enable_time(ms)
		default = 0,preferably not more than 1000ms
		so value range is 0 - 1000.
	*/	
	unsigned int pa_enable_time;
	bool hdmi_ndet;
	int boot_depop;//if found boot pop,set boot_depop 1 test	
#if RESUME_PROBLEM
	int rk610_workstatus;
#endif
	int call_enable;	
	int headset_status;	
};

static void spk_ctrl_fun(int status)
{
	struct rk610_codec_priv *rk610_codec = NULL;
	if(rk610_codec_codec == NULL)
		return;
	rk610_codec = snd_soc_codec_get_drvdata(rk610_codec_codec);
	if(rk610_codec == NULL)
		return;
#ifdef CONFIG_MODEM_SOUND
	if(rk610_codec->call_enable){
		DBG("%s:: is calling cannot set spk\n",__FUNCTION__);
		return;
	}	
#endif		
	if(rk610_codec->spk_ctrl_io)
	{
		DBG("%s:: spk status = %d\n",__FUNCTION__,status);
		gpio_set_value(rk610_codec->spk_ctrl_io, status);
	}
}

/*
 * read rk610 register cache
 */
static inline unsigned int rk610_codec_read_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg)
{
	u16 *cache = codec->reg_cache;
	if (reg > RK610_CACHE_REGNUM)
		return -1;
	return cache[reg];
}

static unsigned int rk610_codec_read(struct snd_soc_codec *codec, unsigned int r)
{
	struct i2c_msg xfer[1];
	u8 reg = r;
	int ret;
	struct i2c_client *i2c = to_i2c_client(codec->dev);

	/* Read register */
	xfer[0].addr = (i2c->addr& 0x60)|(reg);
	xfer[0].flags = I2C_M_RD;
	xfer[0].len = 1;
	xfer[0].buf = &reg;
	xfer[0].scl_rate = 100000;
	ret = i2c_transfer(i2c->adapter, xfer, 1);
	if (ret != 1) {
		dev_err(&i2c->dev, "i2c_transfer() returned %d\n", ret);
		return 0;
	}

	return reg;
}

/*
 * write rk610 register cache
 */
static inline void rk610_codec_write_reg_cache(struct snd_soc_codec *codec,
	unsigned int reg, unsigned int value)
{
	u16 *cache = codec->reg_cache;
	if (reg > RK610_CACHE_REGNUM)
		return;
	cache[reg] = value;
}

static int rk610_codec_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{

	struct rk610_codec_priv *rk610_codec = snd_soc_codec_get_drvdata(rk610_codec_codec);
	u8 data[2];
	struct i2c_client *i2c = to_i2c_client(codec->dev);
#ifdef CONFIG_MODEM_SOUND	
	if(rk610_codec->call_enable)
		return 0;
#endif	
	if(value == rk610_codec_read_reg_cache(codec,reg))
		return 0;
	DBG("Enter::%s, %d, reg=0x%02X, value=0x%02X\n",__FUNCTION__,__LINE__, reg, value);
	data[0] = value & 0x00ff;

	i2c->addr = (i2c->addr & 0x60)|reg;

	if (codec->hw_write(i2c, data, 1) == 1){
//		DBG("================%s %d Run OK================\n",__FUNCTION__,__LINE__);
		rk610_codec_write_reg_cache (codec, reg, value);
		return 0;
	}else{
		DBG("================%s %d Run EIO================\n",__FUNCTION__,__LINE__);
		return -EIO;
	}
}

#ifdef CONFIG_MODEM_SOUND
static int rk610_codec_write_incall(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	u8 data[2];
	struct i2c_client *i2c = to_i2c_client(codec->dev);
	DBG("Enter::%s, %d, reg=0x%02X, value=0x%02X\n",__FUNCTION__,__LINE__, reg, value);
	data[0] = value & 0x00ff;
	rk610_codec_write_reg_cache (codec, reg, value);
	i2c = (struct i2c_client *)codec->control_data;
	i2c->addr = (i2c->addr & 0x60)|reg;

	if (codec->hw_write(i2c, data, 1) == 1)
		return 0;
	else
		return -EIO;
}

void call_set_spk(int on)
{
	struct rk610_codec_priv *rk610_codec;
	if(!rk610_codec_codec)
		return;
	rk610_codec = snd_soc_codec_get_drvdata(rk610_codec_codec);
	if(!rk610_codec)
		return;
		
	switch(on)
	{
	case 0:
		//modem exit call,codec disable loopback
		printk("%s modem exit call \n", __FUNCTION__);		
		rk610_codec_write_incall(rk610_codec_codec,ACCELCODEC_R0E, 0x80);
		rk610_codec->call_enable = 0;	
		break;
	case 1:
		//modem calling,codec enable loopback,spk hp different volume
		printk("%s spk incalling\n", __FUNCTION__);
		rk610_codec->call_enable = 1;	
		rk610_codec_write_incall(rk610_codec_codec,ACCELCODEC_R0E, 0x00);
		return;
	case 2:
		printk("%s hp incalling\n", __FUNCTION__);
		rk610_codec->call_enable = 1;
		rk610_codec_write_incall(rk610_codec_codec,ACCELCODEC_R0E, 0x00);	
		break;
	case 3:
		printk("%s bt incalling\n", __FUNCTION__);    
		rk610_codec->call_enable = 1;	
		rk610_codec_write_incall(rk610_codec_codec,ACCELCODEC_R0E, 0x00);
		break;
	}

	return;
}
#endif
#ifdef CONFIG_RK_HEADSET_DET
//for headset
void rk2928_codec_set_spk(bool on)
{
	struct rk610_codec_priv *rk610_codec;
	if(!rk610_codec_codec)
		return;
	rk610_codec=snd_soc_codec_get_drvdata(rk610_codec_codec);
	if(!rk610_codec)
		return;
		
	if(on)
		rk610_codec->headset_status = HP_IN;
	else
		rk610_codec->headset_status = HP_OUT;
		
	if(rk610_codec->call_enable)
		return;
		
	printk("%s: headset %s %s PA bias_level=%d\n",__FUNCTION__,on?"in":"out",on?"disable":"enable",codec->dapm.bias_level);
	if(on) {
		if(rk610_codec->spk_ctrl_io)
			gpio_set_value(rk610_codec->spk_ctrl_io, GPIO_LOW);
	}
	else {
		if(codec->dapm.bias_level == SND_SOC_BIAS_STANDBY 
		|| codec->dapm.bias_level == SND_SOC_BIAS_OFF){
			return;
		}
		if(rk610_codec->spk_ctrl_io)
			gpio_set_value(rk610_codec->spk_ctrl_io, GPIO_HIGH);
	}
}
#endif

void rk610_codec_reg_read(void)
{
    struct snd_soc_codec *codec = rk610_codec_codec;
    int i;
    unsigned int data;

    for (i=0; i<=0x1f; i++){
        data = rk610_codec_read(codec, i);
        printk("reg[0x%x]=0x%x\n",i,data);
    }
}

struct _coeff_div {
	u32 mclk;
	u32 rate;
	u16 fs;
	u8 sr:5;
	u8 usb:1;
	u8 bclk;
};

/* codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
	/* 8k */
	{12288000, 8000, 1536, 0x6, 0x0,ASC_BCLKDIV_16},
	{11289600, 8000, 1408, 0x16, 0x0,ASC_BCLKDIV_16},
	{18432000, 8000, 2304, 0x7, 0x0,ASC_BCLKDIV_16},
	{16934400, 8000, 2112, 0x17, 0x0,ASC_BCLKDIV_16},
	{8192000, 8000, 1024, 0x0, 0x0,ASC_BCLKDIV_16},
	{12000000, 8000, 1500, 0x6, 0x1,ASC_BCLKDIV_16},

	/* 11.025k */
	{11289600, 11025, 1024, 0x18, 0x0,ASC_BCLKDIV_16},
	{16934400, 11025, 1536, 0x19, 0x0,ASC_BCLKDIV_16},
	{12000000, 11025, 1088, 0x19, 0x1,ASC_BCLKDIV_16},

    /* 12k */
	{12288000, 12000, 1024, 0x8, 0x0,ASC_BCLKDIV_16},
	{18432000, 12000, 1536, 0x9, 0x0,ASC_BCLKDIV_16},
	{12000000, 12000, 1000, 0x8, 0x1,ASC_BCLKDIV_16},

	/* 16k */
	{12288000, 16000, 768, 0xa, 0x0,ASC_BCLKDIV_8},
	{18432000, 16000, 1152, 0xb, 0x0,ASC_BCLKDIV_8},
	{12000000, 16000, 750, 0xa, 0x1,ASC_BCLKDIV_8},

	/* 22.05k */
	{11289600, 22050, 512, 0x1a, 0x0,ASC_BCLKDIV_8},
	{16934400, 22050, 768, 0x1b, 0x0,ASC_BCLKDIV_8},
	{12000000, 22050, 544, 0x1b, 0x1,ASC_BCLKDIV_8},

    /* 24k */
	{12288000, 24000, 512, 0x1c, 0x0,ASC_BCLKDIV_8},
	{18432000, 24000, 768, 0x1d, 0x0,ASC_BCLKDIV_8},
	{12000000, 24000, 500, 0x1c, 0x1,ASC_BCLKDIV_8},

	/* 32k */
	{12288000, 32000, 384, 0xc, 0x0,ASC_BCLKDIV_8},
	{18432000, 32000, 576, 0xd, 0x0,ASC_BCLKDIV_8},
	{12000000, 32000, 375, 0xa, 0x1,ASC_BCLKDIV_8},

	/* 44.1k */
	{11289600, 44100, 256, 0x10, 0x0,ASC_BCLKDIV_4},
	{16934400, 44100, 384, 0x11, 0x0,ASC_BCLKDIV_8},
	{12000000, 44100, 272, 0x11, 0x1,ASC_BCLKDIV_8},

	/* 48k */
	{12288000, 48000, 256, 0x0, 0x0,ASC_BCLKDIV_4},
	{18432000, 48000, 384, 0x1, 0x0,ASC_BCLKDIV_4},
	{12000000, 48000, 250, 0x0, 0x1,ASC_BCLKDIV_4},

	/* 88.2k */
	{11289600, 88200, 128, 0x1e, 0x0,ASC_BCLKDIV_4},
	{16934400, 88200, 192, 0x1f, 0x0,ASC_BCLKDIV_4},
	{12000000, 88200, 136, 0x1f, 0x1,ASC_BCLKDIV_4},

	/* 96k */
	{12288000, 96000, 128, 0xe, 0x0,ASC_BCLKDIV_4},
	{18432000, 96000, 192, 0xf, 0x0,ASC_BCLKDIV_4},
	{12000000, 96000, 125, 0xe, 0x1,ASC_BCLKDIV_4},
};

static inline int get_coeff(int mclk, int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(coeff_div); i++) {
		if (coeff_div[i].rate == rate && coeff_div[i].mclk == mclk)
			return i;
	}

	return -EINVAL;
}

/* The set of rates we can generate from the above for each SYSCLK */

static unsigned int rates_12288[] = {
	8000, 12000, 16000, 24000, 24000, 32000, 48000, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12288 = {
	.count	= ARRAY_SIZE(rates_12288),
	.list	= rates_12288,
};

static unsigned int rates_112896[] = {
	8000, 11025, 22050, 44100,
};

static struct snd_pcm_hw_constraint_list constraints_112896 = {
	.count	= ARRAY_SIZE(rates_112896),
	.list	= rates_112896,
};

static unsigned int rates_12[] = {
	8000, 11025, 12000, 16000, 22050, 2400, 32000, 41100, 48000,
	48000, 88235, 96000,
};

static struct snd_pcm_hw_constraint_list constraints_12 = {
	.count	= ARRAY_SIZE(rates_12),
	.list	= rates_12,
};

static int rk610_codec_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);
	DBG("Enter::%s----%d now_level =%d  old_level = %d\n",__FUNCTION__,__LINE__,level,codec->dapm.bias_level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		/* VREF, VMID=2x50k, digital enabled */
	//	rk610_codec_write(codec, ACCELCODEC_R1D, pwr_reg | 0x0080);
		break;

	case SND_SOC_BIAS_STANDBY:
#if RESUME_PROBLEM	
		if(rk610_codec->rk610_workstatus == SND_SOC_DAPM_STREAM_RESUME)
		{
			DBG("rk610 is resume,have not into standby\n");
			rk610_codec->rk610_workstatus = SND_SOC_DAPM_STREAM_NOP;
			break;
		}
#endif
		printk("rk610 standby\n");
		spk_ctrl_fun(GPIO_LOW);
		rk610_codec_write(codec,ACCELCODEC_R0A, ASC_CLK_DISABLE);
		rk610_codec_write(codec, ACCELCODEC_R1D, 0xFE);
		rk610_codec_write(codec, ACCELCODEC_R1E, 0xFF);
		rk610_codec_write(codec, ACCELCODEC_R1F, 0xFF);
		break;

	case SND_SOC_BIAS_OFF:
		printk("rk610 power off\n");
		spk_ctrl_fun(GPIO_LOW);
		rk610_codec_write(codec,ACCELCODEC_R0A, ASC_CLK_DISABLE);		
		rk610_codec_write(codec, ACCELCODEC_R1D, 0xFF);
		rk610_codec_write(codec, ACCELCODEC_R1E, 0xFF);
		rk610_codec_write(codec, ACCELCODEC_R1F, 0xFF);
		break;
	}

	codec->dapm.bias_level = level;

	return 0;
}

/*
 * Note that this should be called from init rather than from hw_params.
 */
static int rk610_codec_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;

	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);

	DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);

#if RK610_CTL_PLL
	if(rk610_codec_pll_set(freq))
		return -EINVAL;
#endif
	switch (freq) {
	case 11289600:
	case 18432000:
	case 22579200:
	case 36864000:
		rk610_codec->sysclk_constraints = &constraints_112896;
		rk610_codec->sysclk = freq;
		break;

	case 12288000:
	case 16934400:
	case 24576000:
	case 33868800:
		rk610_codec->sysclk_constraints = &constraints_12288;
		rk610_codec->sysclk = freq;
		break;

	case 12000000:
	case 24000000:
		rk610_codec->sysclk_constraints = &constraints_12;
		rk610_codec->sysclk = freq;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rk610_codec_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);
	u16 iface = 0;

//modify 2013-06-27
	if(rk610_codec->pa_enable_time<300)
		spk_ctrl_fun(GPIO_LOW);
	else
		spk_ctrl_fun(GPIO_HIGH);
	rk610_codec_write(codec,ACCELCODEC_R1D, 0x2a);  //setup Vmid and Vref, other module power down
	rk610_codec_write(codec,ACCELCODEC_R1E, 0x40);  ///|ASC_PDASDML_ENABLE);

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		iface = 0x0040;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		iface = 0x0000;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 0x0002;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface |= 0x0001;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface |= 0x0003;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		iface |= 0x0013;
		break;
	default:
		return -EINVAL;
	}

	/* clock inversion */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x0090;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |= 0x0080;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x0010;
		break;
	default:
		return -EINVAL;
	}

	DBG("Enter::%s----%d  iface=%x\n",__FUNCTION__,__LINE__,iface);
	rk610_codec_write(codec, ACCELCODEC_R09, iface);
	return 0;
}

static int rk610_codec_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);
	unsigned int dai_fmt = rtd->card->dai_link[0].dai_fmt;

	u16 iface = rk610_codec_read_reg_cache(codec, ACCELCODEC_R09) & 0x1f3;
	u16 srate = rk610_codec_read_reg_cache(codec, ACCELCODEC_R00) & 0x180;
	int coeff;

	coeff = get_coeff(rk610_codec->sysclk, params_rate(params));
	DBG("Enter::%s----%d  rk610_codec->sysclk=%d coeff = %d\n",__FUNCTION__,__LINE__,rk610_codec->sysclk, coeff);
	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		iface |= 0x0004;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		iface |= 0x0008;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		iface |= 0x000c;
		break;
	}
	DBG("Enter::%s----%d  iface=%x srate =%x rate=%d\n",__FUNCTION__,__LINE__,iface,srate,params_rate(params));

//	rk610_codec_write(codec,ACCELCODEC_R0C, 0x17);
	rk610_codec_write(codec,ACCELCODEC_R04, ASC_INT_MUTE_L|ASC_INT_MUTE_R|ASC_SIDETONE_L_OFF|ASC_SIDETONE_R_OFF);   //soft mute
	//�����Ƚ�clk��EN_INT��disable���������л�bclk��Ƶֵ���ܵ���codec�ڲ�ʱ����ҵ���
	//���ֳ����������ǣ��Ժ�����ֶ���������������Ҿ��������codec��I2S_DATAOUT�Ͽ�Ҳһ��������
	rk610_codec_write(codec,ACCELCODEC_R0B, ASC_DEC_DISABLE|ASC_INT_DISABLE);  //0x00

	/* set iface & srate */
	if ((dai_fmt & SND_SOC_DAIFMT_MASTER_MASK) == SND_SOC_DAIFMT_CBM_CFM)
		iface |= ASC_INVERT_BCLK;//��תBCLK  master״̬�ͳ������˰��ʱ�ӣ�����δ�����������ʱ��������

	rk610_codec_write(codec, ACCELCODEC_R09, iface);
	if (coeff >= 0){
	//    rk610_codec_write(codec, ACCELCODEC_R00, srate|coeff_div[coeff].bclk);
		rk610_codec_write(codec, ACCELCODEC_R0A, (coeff_div[coeff].sr << 1) | coeff_div[coeff].usb|ASC_CLKNODIV|ASC_CLK_ENABLE);
	}
	rk610_codec_write(codec,ACCELCODEC_R0B, gR0BReg);

	return 0;
}

static int rk610_codec_mute(struct snd_soc_dai *dai, int mute)
{
    struct snd_soc_codec *codec = dai->codec;
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);
    printk("Enter::%s----%d--mute=%d\n",__FUNCTION__,__LINE__,mute);

    if (mute)
	{
		rk610_codec_write(codec,ACCELCODEC_R17, 0xFF);  //AOL
		rk610_codec_write(codec,ACCELCODEC_R18, 0xFF);  //AOR
        rk610_codec_write(codec,ACCELCODEC_R19, 0xFF);  //AOM
        rk610_codec_write(codec,ACCELCODEC_R04, ASC_INT_MUTE_L|ASC_INT_MUTE_R|ASC_SIDETONE_L_OFF|ASC_SIDETONE_R_OFF);  //soft mute
	//add for standby
	//	if(!dai->capture_active)
	//	{
	//		rk610_codec_write(codec, ACCELCODEC_R1D, 0xFE);
	//		rk610_codec_write(codec, ACCELCODEC_R1E, 0xFF);
	//		rk610_codec_write(codec, ACCELCODEC_R1F, 0xFF);
	//	}
    }
	else
	{
	//	rk610_codec_write(codec,ACCELCODEC_R1D, 0x2a);  //setup Vmid and Vref, other module power down
	//	rk610_codec_write(codec,ACCELCODEC_R1E, 0x40);  ///|ASC_PDASDML_ENABLE);
		rk610_codec_write(codec,ACCELCODEC_R17, Volume_Codec_PA|ASC_OUTPUT_ACTIVE|ASC_CROSSZERO_EN);  //AOL Volume_Codec_PA|ASC_OUTPUT_ACTIVE|ASC_CROSSZERO_EN);  //AOL
		rk610_codec_write(codec,ACCELCODEC_R18, Volume_Codec_PA|ASC_OUTPUT_ACTIVE|ASC_CROSSZERO_EN); //Volume_Codec_PA|ASC_OUTPUT_ACTIVE|ASC_CROSSZERO_EN);  //AOR
        rk610_codec_write(codec,ACCELCODEC_R04, ASC_INT_ACTIVE_L|ASC_INT_ACTIVE_R|ASC_SIDETONE_L_OFF|ASC_SIDETONE_R_OFF);
		rk610_codec_write(codec,ACCELCODEC_R19, 0x7F);  //AOM

		if(rk610_codec->pa_enable_time == 0)
			msleep(300);
		#if OUT_CAPLESS
    	rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE);
    	#else
    	rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE|ASC_PDPAM_ENABLE);
		#endif
	//	schedule_delayed_work(&rk610_codec->rk610_delayed_work, 0);
	//	rk610_codec_reg_read();
		if(rk610_codec->hdmi_ndet){
			if(rk610_codec->pa_enable_time == 0 )
				spk_ctrl_fun(GPIO_HIGH);
			else if(rk610_codec->pa_enable_time > 0 && rk610_codec->pa_enable_time < 300){
				spk_ctrl_fun(GPIO_HIGH);
				msleep(rk610_codec->pa_enable_time)	;
			}
			else if(rk610_codec->pa_enable_time >=300 && rk610_codec->pa_enable_time < 1000)
				msleep(rk610_codec->pa_enable_time);
		}
    }

    return 0;
}

static void rk610_delayedwork_fun(struct work_struct *work)
{
    struct snd_soc_codec *codec = rk610_codec_codec;
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);	
	
	DBG("--------%s----------\n",__FUNCTION__);
	if(!rk610_codec->boot_depop){
		#if OUT_CAPLESS
		rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE);
		#else
		rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE|ASC_PDPAM_ENABLE);
		#endif
	}
	spk_ctrl_fun(GPIO_HIGH);
}

static struct snd_soc_dai_ops rk610_codec_ops = {
	.hw_params = rk610_codec_pcm_hw_params,
	.set_fmt = rk610_codec_set_dai_fmt,
	.set_sysclk = rk610_codec_set_dai_sysclk,
	.digital_mute = rk610_codec_mute,
};

#define RK610_CODEC_RATES SNDRV_PCM_RATE_8000_96000
#define RK610_CODEC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE |\
							SNDRV_PCM_FMTBIT_S24_LE)

static struct snd_soc_dai_driver rk610_codec_dai = {
	.name = "rk610_codec",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = RK610_CODEC_RATES,
		.formats = RK610_CODEC_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = RK610_CODEC_RATES,
		.formats = RK610_CODEC_FORMATS,
	 },
	.ops = &rk610_codec_ops,
	.symmetric_rates = 1,
};

static int rk610_codec_suspend(struct snd_soc_codec *codec)
{
	DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	rk610_codec_set_bias_level(codec, SND_SOC_BIAS_OFF);
//	rk610_codec_reg_read();
	return 0;
}

static int rk610_codec_resume(struct snd_soc_codec *codec)
{
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);

	DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);
	/* Sync reg_cache with the hardware */
	
//	rk610_codec_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
#if RESUME_PROBLEM
	rk610_codec->rk610_workstatus = SND_SOC_DAPM_STREAM_RESUME;
#endif
	return 0;
}

#define USE_MIC_IN
#define USE_LPF
void rk610_codec_reg_set(void)
{
    struct snd_soc_codec *codec = rk610_codec_codec;
	struct rk610_codec_priv *rk610_codec =snd_soc_codec_get_drvdata(codec);	

    unsigned int digital_gain;
	unsigned int mic_vol = Volume_Input;
	rk610_codec_write(codec,ACCELCODEC_R1D, 0x30);
	rk610_codec_write(codec,ACCELCODEC_R1E, 0x40);

#ifdef USE_LPF
	// Route R-LPF->R-Mixer, L-LPF->L-Mixer
	rk610_codec_write(codec,ACCELCODEC_R15, 0xC1);
#else
	// Route RDAC->R-Mixer, LDAC->L->Mixer
	rk610_codec_write(codec,ACCELCODEC_R15, 0x0C);
#endif
	// With Cap Output, VMID ramp up slow
	rk610_codec_write(codec,ACCELCODEC_R1A, 0x14);
    mdelay(10);

	rk610_codec_write(codec,ACCELCODEC_R0C, 0x10|ASC_INPUT_VOL_0DB);   //LIL
    rk610_codec_write(codec,ACCELCODEC_R0D, 0x10|ASC_INPUT_VOL_0DB);   //LIR

#ifdef USE_MIC_IN
	if(mic_vol > 0x07)
	{
		rk610_codec_write(codec,ACCELCODEC_R12, 0x4c|ASC_MIC_INPUT|ASC_MIC_BOOST_20DB);   //Select MIC input
		mic_vol -= 0x07;
	}	
	else
		rk610_codec_write(codec,ACCELCODEC_R12, 0x4c|ASC_MIC_INPUT);   //Select MIC input
    rk610_codec_write(codec,ACCELCODEC_R1C, ASC_DEM_ENABLE);  //0x00);  //use default value
#else
    rk610_codec_write(codec,ACCELCODEC_R12, 0x4c);   //Select Line input
#endif

    rk610_codec_write(codec,ACCELCODEC_R0E, 0x10|mic_vol);   //MIC
	
	// Diable route PGA->R/L Mixer, PGA gain 0db.
    rk610_codec_write(codec,ACCELCODEC_R13, 0x05 | 0 << 3);
    rk610_codec_write(codec,ACCELCODEC_R14, 0x05 | 0 << 3);

    //2soft mute
    rk610_codec_write(codec,ACCELCODEC_R04, ASC_INT_MUTE_L|ASC_INT_MUTE_R|ASC_SIDETONE_L_OFF|ASC_SIDETONE_R_OFF);   //soft mute

    //2set default SR and clk
    rk610_codec_write(codec,ACCELCODEC_R0A, ASC_NORMAL_MODE|(0x10 << 1)|ASC_CLKNODIV|ASC_CLK_DISABLE);
    gR0AReg = ASC_NORMAL_MODE|(0x10 << 1)|ASC_CLKNODIV|ASC_CLK_DISABLE;
    //2Config audio  interface
    rk610_codec_write(codec,ACCELCODEC_R09, ASC_I2S_MODE|ASC_16BIT_MODE|ASC_NORMAL_LRCLK|ASC_LRSWAP_DISABLE|ASC_NORMAL_BCLK);
    rk610_codec_write(codec,ACCELCODEC_R00, ASC_HPF_ENABLE|ASC_DSM_MODE_ENABLE|ASC_SCRAMBLE_ENABLE|ASC_DITHER_ENABLE|ASC_BCLKDIV_4);
    //2volume,input,output
    digital_gain = Volume_Output;
	
	if(rk610_codec_read(codec,ACCELCODEC_R05)!=0x0f) {
		rk610_codec_write(codec,ACCELCODEC_R05, (digital_gain >> 8) & 0xFF);
		rk610_codec_write(codec,ACCELCODEC_R06, digital_gain & 0xFF);
	}
	if(rk610_codec_read(codec,ACCELCODEC_R07)!=0x0f){
		rk610_codec_write(codec,ACCELCODEC_R07, (digital_gain >> 8) & 0xFF);
		rk610_codec_write(codec,ACCELCODEC_R08, digital_gain & 0xFF);
	}
    rk610_codec_write(codec,ACCELCODEC_R0B, ASC_DEC_ENABLE|ASC_INT_ENABLE);
    gR0BReg = ASC_DEC_ENABLE|ASC_INT_ENABLE;  //ASC_DEC_DISABLE|ASC_INT_ENABLE;

	if(rk610_codec->boot_depop){
		#if OUT_CAPLESS
		rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE);
		#else
		rk610_codec_write(codec,ACCELCODEC_R1F, 0x09|ASC_PDMIXM_ENABLE|ASC_PDPAM_ENABLE);
		#endif
	}	
}

static int rk610_codec_probe(struct snd_soc_codec *codec)
{
	struct rk610_codec_priv *rk610_codec = snd_soc_codec_get_drvdata(codec);
	int ret;

	rk610_codec_codec = codec;
	DBG("[%s] start\n", __FUNCTION__);
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, rk610_codec->control_type);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
		return ret;
	}

	//For RK610, i2c write&read method is special, do not use system default method.
	codec->write = rk610_codec_write;
	codec->read = rk610_codec_read;
	codec->hw_write = (hw_write_t)i2c_master_send;

	if (rk610_codec_codec == NULL) {
		dev_err(codec->dev, "Codec device not registered\n");
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&rk610_codec->rk610_delayed_work, rk610_delayedwork_fun);

	if(rk610_codec->spk_ctrl_io)
	{
		ret = gpio_request(rk610_codec->spk_ctrl_io, "rk610 spk_ctrl");
	    if (ret){
	        printk("rk610_control request gpio fail!\n");
			return ret;
	    }
	    gpio_direction_output(rk610_codec->spk_ctrl_io, GPIO_LOW);
	    gpio_set_value(rk610_codec->spk_ctrl_io, GPIO_LOW);	
	}

	rk610_codec->hdmi_ndet = true;
	rk610_codec->call_enable = 0;
	rk610_codec->headset_status = HP_OUT;
#if RESUME_PROBLEM	
	rk610_codec->rk610_workstatus = SND_SOC_DAPM_STREAM_NOP;
#endif
	
    rk610_control_init_codec();
    rk610_codec_reg_set();
//	rk610_codec_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	schedule_delayed_work(&rk610_codec->rk610_delayed_work, msecs_to_jiffies(1000));

	codec->dapm.bias_level = SND_SOC_BIAS_PREPARE;
	return ret;
}

/* power down chip */
static int rk610_codec_remove(struct snd_soc_codec *codec)
{
	rk610_codec_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_rk610_codec = {
	.probe =	rk610_codec_probe,
	.remove =	rk610_codec_remove,
	.suspend =	rk610_codec_suspend,
	.resume =	rk610_codec_resume,
	.set_bias_level = rk610_codec_set_bias_level,
//	.volatile_register = wm8900_volatile_register,
	.reg_cache_size = ARRAY_SIZE(rk610_codec_reg),
	.reg_word_size = sizeof(u16),
	.reg_cache_default = rk610_codec_reg,
//	.dapm_widgets = rk610_codec_dapm_widgets,
//	.num_dapm_widgets = ARRAY_SIZE(rk610_codec_dapm_widgets),
//	.dapm_routes = audio_map,
//	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
static int rk610_codec_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct rk610_codec_priv *rk610_codec;
	struct device_node *rk610_np = i2c->dev.of_node;
	int ret;
	int val = 0;
	DBG("%s start\n", __FUNCTION__);
	rk610_codec = kzalloc(sizeof(struct rk610_codec_priv), GFP_KERNEL);
	if (rk610_codec == NULL)
		return -ENOMEM;

	if(!of_property_read_u32(rk610_np, "boot_depop", &val))
		rk610_codec->boot_depop = val;
	if(!of_property_read_u32(rk610_np, "pa_enable_time", &val))
		rk610_codec->pa_enable_time = val;
	if(rk610_codec->pa_enable_time > 1000)
		rk610_codec->pa_enable_time = 1000;
		
	rk610_codec->spk_ctrl_io = of_get_named_gpio(rk610_np,"spk_ctl_io", 0);
	if (!gpio_is_valid(rk610_codec->spk_ctrl_io)){
		printk("invalid core_info->reset_gpio: %d\n",rk610_codec->spk_ctrl_io);
		return -1;
	}

	i2c_set_clientdata(i2c, rk610_codec);
	rk610_codec->control_type = SND_SOC_I2C;

	ret =  snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_rk610_codec, &rk610_codec_dai, 1);
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
		kfree(rk610_codec);
	}
	return ret;
}

static int rk610_codec_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct i2c_device_id rk610_codec_i2c_id[] = {
	{ "rk610_codec", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rk610_codec_i2c_id);

/* corgi i2c codec control layer */
static struct i2c_driver rk610_codec_i2c_driver = {
	.driver = {
		.name = "rk610_codec",
		.owner = THIS_MODULE,
	},
	.probe = rk610_codec_i2c_probe,
	.remove = rk610_codec_i2c_remove,
	.id_table = rk610_codec_i2c_id,
};
#endif

static int __init rk610_codec_modinit(void)
{
	int ret;
	DBG("[%s] start\n", __FUNCTION__);
	ret = i2c_add_driver(&rk610_codec_i2c_driver);
	if (ret != 0)
		pr_err("rk610 codec: Unable to register I2C driver: %d\n", ret);
	return ret;
}
module_init(rk610_codec_modinit);

static void __exit rk610_codec_exit(void)
{
	i2c_del_driver(&rk610_codec_i2c_driver);
}
module_exit(rk610_codec_exit);

MODULE_DESCRIPTION("ASoC RK610 CODEC driver");
MODULE_AUTHOR("rk@rock-chips.com");
MODULE_LICENSE("GPL");

//=====================================================================
//Proc
#ifdef RK610_PROC
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/vmalloc.h>
static ssize_t rk610_reg_write(struct file *file,
		const char __user *buffer, size_t len, loff_t *ppos)
{
	char *cookie_pot; 
	char *p;
	int reg;
	int value;
	struct rk610_codec_priv *rk610_codec = snd_soc_codec_get_drvdata(rk610_codec_codec);

	cookie_pot = (char *)vmalloc( len );
	if (!cookie_pot) 
	{
		return -ENOMEM;
	} 
	else 
	{
		if (copy_from_user( cookie_pot, buffer, len )) 
			return -EFAULT;
	}

	switch(cookie_pot[0])
	{
	case 'p':
		spk_ctrl_fun(GPIO_HIGH);
		break;
	case 'o':
		spk_ctrl_fun(GPIO_LOW);
		break;	
	case 'r':
	case 'R':
		printk("Read reg debug\n");		
		if(cookie_pot[1] ==':')
		{
			strsep(&cookie_pot,":");
			while((p=strsep(&cookie_pot,",")))
			{
				reg = simple_strtol(p,NULL,16);
				value = rk610_codec_read(rk610_codec_codec,reg);
				printk("wm8994_read:0x%04x = 0x%04x\n",reg,value);
			}
			printk("\n");
		}
		else
		{
			printk("Error Read reg debug.\n");
			printk("For example: echo 'r:22,23,24,25'>wm8994_ts\n");
		}
		break;
	case 'w':
	case 'W':
		printk("Write reg debug\n");		
		if(cookie_pot[1] ==':')
		{
			strsep(&cookie_pot,":");
			while((p=strsep(&cookie_pot,"=")))
			{
				reg = simple_strtol(p,NULL,16);
				p=strsep(&cookie_pot,",");
				value = simple_strtol(p,NULL,16);
				rk610_codec_write(rk610_codec_codec,reg,value);
				printk("wm8994_write:0x%04x = 0x%04x\n",reg,value);
			}
			printk("\n");
		}
		else
		{
			printk("Error Write reg debug.\n");
			printk("For example: w:22=0,23=0,24=0,25=0\n");
		}
		break;	
	case 'D' :
		printk("Dump reg\n");
		rk610_codec_reg_read();
		break;
	case 't' :
		printk("old pa_enable_time = %d\n",rk610_codec->pa_enable_time);
		if(cookie_pot[1] ==':')
		{
			strsep(&cookie_pot,":");
			p=strsep(&cookie_pot," ");
			rk610_codec->pa_enable_time = simple_strtol(p,NULL,10);
			printk("new pa_enable_time = %d\n",rk610_codec->pa_enable_time);
		}
		break;		
	}

	return len;
}
static int proc_reg_show(struct seq_file *s, void *v)
{

	return 0;
}
static int proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_reg_show, NULL);
}

static const struct file_operations proc_i2s_fops = {
	.open		= proc_open,
	.read		= seq_read,
	.write = rk610_reg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};
static int __init rk610_proc_init(void)
{
	proc_create("driver/rk610_ts", 0, NULL, &proc_i2s_fops);
	return 0;
}
late_initcall(rk610_proc_init);
#endif
