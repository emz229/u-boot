/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2009, Intel Corporation.
 * Modified for uboot by Eric Schikschneit (eric.schikschneit@novatechautomation.com)
 */
#ifndef __PSB_INTEL_REG_H__
#define __PSB_INTEL_REG_H__

struct intel_gmbus {
	u32 base;
	int running;
	// struct i2c_adapter adapter;
	// struct i2c_adapter *force_bit;
	u32 reg0;
};

/* PCI Configuration Space (D02:F0): GMBus */
#define GMB_BASE	0x10 /* GTTMMADR BAR*/

#define GMBUS0			0x5100 /* clock/port select */
#define   GMBUS_RATE_100KHZ	(0<<8)
#define   GMBUS_RATE_50KHZ	(1<<8)
#define   GMBUS_RATE_400KHZ	(2<<8) /* reserved on Pineview */
#define   GMBUS_RATE_1MHZ	(3<<8) /* reserved on Pineview */
#define   GMBUS_HOLD_EXT	(1<<7) /* 300ns hold time, rsvd on Pineview */
#define   GMBUS_PORT_DISABLED	0
#define   GMBUS_PORT_SSC	1
#define   GMBUS_PORT_VGADDC	2
#define   GMBUS_PORT_PANEL	3
#define   GMBUS_PORT_DPC	4 /* HDMIC */
#define   GMBUS_PORT_DPB	5 /* SDVO, HDMIB */
				  /* 6 reserved */
#define   GMBUS_PORT_DPD	7 /* HDMID */
#define   GMBUS_NUM_PORTS       8
#define GMBUS1			0x5104 /* command/status */
#define   GMBUS_SW_CLR_INT	(1<<31)
#define   GMBUS_SW_RDY		(1<<30)
#define   GMBUS_ENT		(1<<29) /* enable timeout */
#define   GMBUS_CYCLE_NONE	(0<<25)
#define   GMBUS_CYCLE_WAIT	(1<<25)
#define   GMBUS_CYCLE_INDEX	(2<<25)
#define   GMBUS_CYCLE_STOP	(4<<25)
#define   GMBUS_BYTE_COUNT_SHIFT 16
#define   GMBUS_SLAVE_INDEX_SHIFT 8
#define   GMBUS_SLAVE_ADDR_SHIFT 1
#define   GMBUS_SLAVE_READ	(1<<0)
#define   GMBUS_SLAVE_WRITE	(0<<0)
#define GMBUS2			0x5108 /* status */
#define   GMBUS_INUSE		(1<<15)
#define   GMBUS_HW_WAIT_PHASE	(1<<14)
#define   GMBUS_STALL_TIMEOUT	(1<<13)
#define   GMBUS_INT		(1<<12)
#define   GMBUS_HW_RDY		(1<<11)
#define   GMBUS_SATOER		(1<<10)
#define   GMBUS_ACTIVE		(1<<9)
#define GMBUS3			0x510c /* data buffer bytes 3-0 */
#define GMBUS4			0x5110 /* interrupt mask (Pineview+) */
#define   GMBUS_SLAVE_TIMEOUT_EN (1<<4)
#define   GMBUS_NAK_EN		(1<<3)
#define   GMBUS_IDLE_EN		(1<<2)
#define   GMBUS_HW_WAIT_EN	(1<<1)
#define   GMBUS_HW_RDY_EN	(1<<0)
#define GMBUS5			0x5120 /* byte index */
#define   GMBUS_2BYTE_INDEX_EN	(1<<31)

#endif
