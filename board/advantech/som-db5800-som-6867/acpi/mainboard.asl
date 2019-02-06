/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2016, Bin Meng <bmeng.cn@gmail.com>
 */

/* Power Button */
Device (PWRB)
{
	Name(_HID, EISAID("PNP0C0C"))
}

Device (TPM)
{
	//
	// Define _HID, "PNP0C31" is defined in
	// "Secure Startup-FVE and TPM Admin BIOS and Platform Requirements"
	//
	Name (_HID, EISAID ("PNP0C31"))

	//
	// Readable name of this device, don't know if this way is correct yet
	//
	Name (_STR, Unicode ("TPM 1.2 Device"))

	//
	// Return the resource consumed by TPM device
	//
	Name (_CRS, ResourceTemplate () {
		Memory32Fixed (ReadWrite, 0xfed40000, 0x5000)
	})

	//
	// Operational region for TPM access
	//
	OperationRegion (TPMR, SystemMemory, 0xfed40000, 0x5000)
	Field (TPMR, AnyAcc, NoLock, Preserve)
	{
		ACC0, 8,
	}

	//
	// Operational region for TPM support, TPM Physical Presence and TPM Memory Clear
	// Region Offset 0xFFFF0000 and Length 0xF0 will be fixed in C code.
	//
	OperationRegion (TNVS, SystemMemory, 0xFFFF0000, 0xF0)
	Field (TNVS, AnyAcc, NoLock, Preserve)
	{
		PPIN,   8,  //   Software SMI for Physical Presence Interface
		PPIP,   32, //   Used for save physical presence paramter
		PPRP,   32, //   Physical Presence request operation response
		PPRQ,   32, //   Physical Presence request operation
		LPPR,   32, //   Last Physical Presence request operation
		FRET,   32, //   Physical Presence function return code
		MCIN,   8,  //   Software SMI for Memory Clear Interface
		MCIP,   32, //   Used for save the Mor paramter
		MORD,   32, //   Memory Overwrite Request Data
		MRET,   32  //   Memory Overwrite function return code
	}
}

scope (\_SB.PCI0)
{
	Device(SBUS)
	{
		Name (_ADR, 0x001F0003)
	}

	Device(NIO1)
	{
		Name(_HID, "PCA9554")
		Name(_CID, "PCA9554")
		Name(_UID, 1)

		Name(RBUF, ResourceTemplate()
		{
			I2CSerialBus(0x20, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.PCI0.SBUS", 0, ResourceConsumer, , )
		})
		Method(_CRS, 0x0, NotSerialized)
		{
			Return(RBUF)
		}
		Method(_STA, 0x0, NotSerialized)
		{
			Return(0xf)
		}
	}

	Device(MAX1)
	{
		Name(_HID, "MAX6652")
		Name(_CID, "MAX6652")
		Name(_UID, 2)

		Name(RBUF, ResourceTemplate()
		{
			I2CSerialBus(0x14, ControllerInitiated, 400000, AddressingMode7Bit, "\\_SB.PCI0.SBUS", 0, ResourceConsumer, , )
		})
		Method(_CRS, 0x0, NotSerialized)
		{
			Return(RBUF)
		}
		Method(_STA, 0x0, NotSerialized)
		{
			Return(0xf)
		}
	}
}
