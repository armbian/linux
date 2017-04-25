obj-$(CONFIG_MAC80211) += mac80211.o

# mac80211 objects
mac80211-y := \
	main.o status.o \
	sta_info.o \
	wep.o \
	wpa.o \
	wapi.o \
	scan.o offchannel.o \
	ht.o agg-tx.o agg-rx.o \
	ibss.o \
	mlme.o work.o \
	iface.o \
	rate.o \
	michael.o \
	tkip.o \
	aes_ccm.o \
	aes_cmac.o \
	cfg.o \
	rx.o \
	spectmgmt.o \
	tx.o \
	key.o \
	util.o \
	wme.o \
	event.o \
	chan.o

mac80211-$(CONFIG_MAC80211_LEDS) += led.o
mac80211-$(CONFIG_MAC80211_DEBUGFS) += \
	debugfs.o \
	debugfs_sta.o \
	debugfs_netdev.o \
	debugfs_key.o

mac80211-$(CONFIG_MAC80211_MESH) += \
	mesh.o \
	mesh_pathtbl.o \
	mesh_plink.o \
	mesh_hwmp.o

mac80211-$(CONFIG_PM) += pm.o

mac80211-$(CONFIG_MAC80211_DRIVER_API_TRACER) += driver-trace.o
CFLAGS_driver-trace.o := -I$(src)

# objects for PID algorithm
rc80211_pid-y := rc80211_pid_algo.o
rc80211_pid-$(CONFIG_MAC80211_DEBUGFS) += rc80211_pid_debugfs.o

rc80211_minstrel-y := rc80211_minstrel.o
rc80211_minstrel-$(CONFIG_MAC80211_DEBUGFS) += rc80211_minstrel_debugfs.o

rc80211_minstrel_ht-y := rc80211_minstrel_ht.o
rc80211_minstrel_ht-$(CONFIG_MAC80211_DEBUGFS) += rc80211_minstrel_ht_debugfs.o

mac80211-$(CONFIG_MAC80211_RC_PID) += $(rc80211_pid-y)
mac80211-$(CONFIG_MAC80211_RC_MINSTREL) += $(rc80211_minstrel-y)
mac80211-$(CONFIG_MAC80211_RC_MINSTREL_HT) += $(rc80211_minstrel_ht-y)

ccflags-y += -D__CHECK_ENDIAN__
# Extra IE for probe response from upper layer is needed in P2P GO
# For offloading probe response to FW, the extra IE must be included
# in the probe response template
ccflags-y += -DPROBE_RESP_EXTRA_IE
ccflags-y += -DIPV6_FILTERING
#ccflags-y += -DCONFIG_MAC80211_XR_ROAMING_CHANGES
