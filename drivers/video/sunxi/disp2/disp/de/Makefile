ifeq ($(CONFIG_ARCH_SUN50IW1P1),y)
sub_dir = lowlevel_sun50iw1
endif

ifeq ($(CONFIG_ARCH_SUN8IW10),y)
sub_dir = lowlevel_sun8iw10
endif

ifeq ($(CONFIG_ARCH_SUN8IW11),y)
sub_dir = lowlevel_v2x
endif

ifeq ($(CONFIG_ARCH_SUN50IW2),y)
sub_dir = lowlevel_v2x
endif

-include $(obj)/de/lowlevel_sun50iw1/Makefile
-include $(obj)/de/lowlevel_sun8iw10/Makefile
-include $(obj)/de/lowlevel_sun8iw11/Makefile
-include $(obj)/de/lowlevel_v2x/Makefile
