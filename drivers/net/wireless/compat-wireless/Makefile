NOSTDINC_FLAGS := -I$(srctree)/drivers/net/wireless/compat-wireless/include/ \
	-include $(srctree)/drivers/net/wireless/compat-wireless/include/linux/compat-2.6.h \
	-DCOMPAT_STATIC

obj-y := compat/
obj-$(CONFIG_COMPAT_WIRELESS) += net/wireless/ net/mac80211/
obj-$(CONFIG_XRADIO) += drivers/xradio/

clean-files += Module.symvers Module.markers modules modules.order
