ifneq ($(strip $(CONFIG_ARCH_SUN50I) $(CONFIG_ARCH_SUN8IW10) $(CONFIG_ARCH_SUN8IW11)),)
obj-$(CONFIG_PM_SLEEP) += pm/
endif

obj-$(CONFIG_SUNXI_SRAM) += sunxi_sram.o

obj-$(CONFIG_ARCH_SUNXI) += sunxi-sid.o

#ccflags-y += -DDEBUG
