xradio_core-y := \
	common.o \
	fwio.o \
	txrx.o \
	main.o \
	queue.o \
	hwio.o \
	bh.o \
	wsm.o \
	sta.o \
	ap.o \
	scan.o \
	platform.o

xradio_core-$(CONFIG_PM)            += pm.o
xradio_core-$(CONFIG_XRADIO_SDIO)   += sdio.o
xradio_core-$(CONFIG_XRADIO_DEBUG)  += debug.o
xradio_core-$(CONFIG_XRADIO_ITP)    += itp.o

ccflags-y += -DP2P_MULTIVIF
ccflags-y += -DMCAST_FWDING
ccflags-y += -DXRADIO_SUSPEND_RESUME_FILTER_ENABLE
ccflags-y += -DAP_AGGREGATE_FW_FIX
ccflags-y += -DAP_HT_CAP_UPDATE
ccflags-y += -DHW_ERROR_WIFI_RESET
ccflags-y += -DAP_HT_COMPAT_FIX
ccflags-y += -DMONITOR_MODE
ccflags-y += -DTCP_SIZE_ADJUST

# Use vfs for firmware load when request_firmware
# can't work on other platform.
ccflags-y += -DUSE_VFS_FIRMWARE

# Extra IE for probe response from upper layer is needed in P2P GO
# For offloading probe response to FW, the extra IE must be included
# in the probe response template
ccflags-y += -DPROBE_RESP_EXTRA_IE
ccflags-y += -DIPV6_FILTERING

# Modified for P2P stability.
ccflags-y += -DTES_P2P_0002_ROC_RESTART
ccflags-y += -DTES_P2P_000B_EXTEND_INACTIVITY_CNT
ccflags-y += -DTES_P2P_000B_DISABLE_EAPOL_FILTER

# Modified for power save.
ccflags-y += -DXRADIO_USE_LONG_DTIM_PERIOD
ccflags-y += -DXRADIO_USE_LONG_KEEP_ALIVE_PERIOD

# Mac addr config, disable hex for default.
#ccflags-y += -DXRADIO_MACPARAM_HEX

# Use semaphore to sync bh txrx.
#ccflags-y += -DBH_USE_SEMAPHORE

ldflags-y += --strip-debug

xradio_wlan-y := wlan_init.o

obj-$(CONFIG_XRADIO) += xradio_core.o
obj-$(CONFIG_XRADIO) += xradio_wlan.o

