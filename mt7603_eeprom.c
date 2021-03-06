/*
 * Copyright (C) 2016 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mt7603.h"
#include "mt7603_eeprom.h"

static int
mt7603_efuse_read(struct mt7603_dev *dev, u32 base, u16 addr, u8 *data)
{
	u32 val;
	int i;

	val = mt76_rr(dev, base + MT_EFUSE_CTRL);
	val &= ~(MT_EFUSE_CTRL_AIN |
		 MT_EFUSE_CTRL_MODE);
	val |= MT76_SET(MT_EFUSE_CTRL_AIN, addr & ~0xf);
	val |= MT_EFUSE_CTRL_KICK;
	mt76_wr(dev, base + MT_EFUSE_CTRL, val);

	if (!mt76_poll(dev, base + MT_EFUSE_CTRL, MT_EFUSE_CTRL_KICK, 0, 1000))
		return -ETIMEDOUT;

	udelay(2);

	val = mt76_rr(dev, base + MT_EFUSE_CTRL);
	if ((val & MT_EFUSE_CTRL_AOUT) == MT_EFUSE_CTRL_AOUT ||
	    WARN_ON_ONCE(!(val & MT_EFUSE_CTRL_VALID))) {
		memset(data, 0xff, 16);
		return 0;
	}

	for (i = 0; i < 4; i++) {
	    val = mt76_rr(dev, base + MT_EFUSE_RDATA(i));
	    put_unaligned_le32(val, data + 4 * i);
	}

	return 0;
}

static int
mt7603_efuse_init(struct mt7603_dev *dev)
{
	u32 base = mt7603_reg_map(dev, MT_EFUSE_BASE);
	int len = MT7603_EEPROM_SIZE;
	void *buf;
	int ret, i;

	if (mt76_rr(dev, base + MT_EFUSE_BASE_CTRL) & MT_EFUSE_BASE_CTRL_EMPTY)
		return 0;

	dev->mt76.otp.data = devm_kzalloc(dev->mt76.dev, len, GFP_KERNEL);
	dev->mt76.otp.size = len;
	if (!dev->mt76.otp.data)
		return -ENOMEM;

	buf = dev->mt76.otp.data;
	for (i = 0; i + 16 <= len; i += 16) {
		ret = mt7603_efuse_read(dev, base, i, buf + i);
		if (ret)
			return ret;
	}

	return 0;
}

static bool
mt7603_has_cal_free_data(struct mt7603_dev *dev, u8 *efuse)
{
	if (!efuse[MT_EE_TEMP_SENSOR_CAL])
		return false;

	if (get_unaligned_le16(efuse + MT_EE_TX_POWER_0_START_2G) == 0)
		return false;

    if (get_unaligned_le16(efuse + MT_EE_TX_POWER_1_START_2G) == 0)
        return false;

	if (!efuse[MT_EE_CP_FT_VERSION])
		return false;

	if (!efuse[MT_EE_XTAL_FREQ_OFFSET])
		return false;

	if (!efuse[MT_EE_XTAL_WF_RFCAL])
		return false;

	return true;
}


static void
mt7603_apply_cal_free_data(struct mt7603_dev *dev, u8 *efuse)
{
	static const u8 cal_free_bytes[] = {
		MT_EE_CHIP_ID,
		MT_EE_TEMP_SENSOR_CAL,
		MT_EE_NIC_CONF_0,
		MT_EE_NIC_CONF_1,
		MT_EE_NIC_CONF_2,
		MT_EE_TX_POWER_0_START_2G,
		MT_EE_TX_POWER_0_START_2G + 1,
		MT_EE_TX_POWER_1_START_2G,
		MT_EE_TX_POWER_1_START_2G + 1,
		MT_EE_CP_FT_VERSION,
		MT_EE_XTAL_FREQ_OFFSET,
		MT_EE_XTAL_WF_RFCAL,
	};
	u8 *eeprom = dev->mt76.eeprom.data;
	int n = ARRAY_SIZE(cal_free_bytes);
	int i;

	if (!mt7603_has_cal_free_data(dev, efuse))
		return;

//	if (is_mt7628(dev))
//		n -= 2;
	eeprom[0x34]=0x11;
	eeprom[0x42]=0x11;
	printk("mt76:mt7603_eeprom.c:: writing eeprom to efuse\n");
	for (i = 0; i < 0x140; i++) {
		printk("mt76:mt7603_eeprom.c:: offset=0x%03x, eeprom=0x%03x\n", i, eeprom[i]);
		efuse[i]=eeprom[i];
	}

	printk("mt76:mt7603_eeprom.c:: reading efuse values for the defined cal_free_bytes\n");
	for (i = 0; i < n; i++) {
		int offset = cal_free_bytes[i];
		// eeprom[offset] = efuse[offset];
		printk("mt76:mt7603_eeprom.c:: offset=0x%03x, efuse=0x%03x\n", offset, efuse[offset]);
	}
}


static int
mt7603_eeprom_load(struct mt7603_dev *dev)
{
	int ret;

	ret = mt76_eeprom_init(&dev->mt76, MT7603_EEPROM_SIZE);
	if (ret < 0)
		return ret;

	return mt7603_efuse_init(dev);
}

int mt7603_eeprom_init(struct mt7603_dev *dev)
{
	int ret;

	ret = mt7603_eeprom_load(dev);
	if (ret < 0)
		return ret;

	dev->mt76.cap.has_2ghz = true;
	memcpy(dev->mt76.macaddr, dev->mt76.eeprom.data + MT_EE_MAC_ADDR,
	       ETH_ALEN);

	mt7603_apply_cal_free_data(dev, dev->mt76.otp.data);
	mt76_eeprom_override(&dev->mt76);

	return 0;
}
