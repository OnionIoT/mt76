EXTRA_CFLAGS += -Werror

obj-m := mt7603e.o


mt7603e-y := \
	mmio.o util.o trace.o dma.o mac80211.o debugfs.o eeprom.o tx.o \
	mt7603_pci.o mt7603_soc.o mt7603_main.o mt7603_init.o mt7603_mcu.o \
	mt7603_core.o mt7603_dma.o mt7603_mac.o mt7603_eeprom.o \
	mt7603_beacon.o mt7603_debugfs.o
