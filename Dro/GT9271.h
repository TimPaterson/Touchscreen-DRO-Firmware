//****************************************************************************
// GT9271.h
//
// Created 11/14/2023 5:29:29 PM by Tim
//
//****************************************************************************

#pragma once

#include <Com/I2c.h>
#include <Timer/TimerLib.h>


static constexpr int I2cBitRate = 400'000;

class Gt9271 : public TouchMgr, public DECLARE_I2C(SERCOM1, I2cBitRate)
{
protected:
	static constexpr int TouchUpdateRate = 30;	// full touch scans per second
	static constexpr int MaxTouchPoints = 1;	// chip supports 10
	static constexpr byte I2cAddrLoReset = 0xBA;
	static constexpr byte I2cAddrHiReset = 0x28;
	static constexpr int I2cAddr = I2cAddrLoReset;
	static constexpr uint ProductId = '9' | ('2' << 8) | ('7' << 16) | ('1' << 24);
	
public:
	// Types
	enum Registers
	{
		REG_Command = 0x8040,
		
		REG_ProdId0 = 0x8140,
		REG_ProdId1,
		REG_ProdId2,
		REG_ProdId3,
		REG_FirmwareLo,
		REG_FirmwareHi,
		REG_XresLo,
		REG_XresHi,
		REG_YresLo,
		REG_YresHi,
		REG_VendorId,
		
		REG_ConfigStart = 0x8047,
		
		REG_TouchCoords = 0x814E,
	};
	
	enum I2cState
	{
		I2CSTATE_Ready,
		I2CSTATE_GetData,
		I2CSTATE_ResetFlag,
	};
	
	struct Coordinate
	{
		byte	reserved;
		byte	trackId;
		ushort	X;
		ushort	Y;
		ushort	size;
	};
	
	union TouchCoords
	{
		struct  
		{
			byte	touchCount:4;
			byte	haveKey:1;
			byte	:1;
			byte	largeDetect:1;
			byte	bufStatus:1;
		};
		
		Coordinate	Coordinates[MaxTouchPoints];
	};
	
// Macro initializes memory with register value in correct byte order
#define REGISTER(x)	HIBYTE(x), LOBYTE(x)
	
public:
	bool Init()
	{
		uint	status;
		
		// PA19 is initialized as an output for CTP reset, then muxed
		// to SERCOM1 for RTP MISO. We just need to turn the mux off
		// to enable as the reset output again.
		SetPinConfigA(0, CtpReset_BIT);
		
		// TouchIrq_PIN needs to be made an output and driven low during reset
		ClearTouchIrq();
		SetTouchIrqOut();
		SetPinConfigB(0, TouchIrq_BIT);
		
		// Reset the GT9271. After reset, it will read TouchIrq_PIN
		// (which we just set low) to set its I2C address.
		ClearCtpReset();	// active-low reset
		Timer::Delay_us(100);
		SetCtpReset();
		Timer::Delay_ms(50 + 5);
		// Re-enable TouchIrq_PIN as input muxed to EXINT 16
		SetPinConfigB(PORT_PINCFG_INEN | PORT_PINCFG_PMUXEN | PORT_PINCFG_PULLEN, TouchIrq_BIT);
		SetTouchIrq();
		SetTouchIrqIn();	
		
		I2cInit();
		Enable();
		status = WriteRead(IdRegister, sizeof IdRegister, m_inBuf.idBytes, sizeof m_inBuf.idBytes);
		if (IsStatusOk(status) && m_inBuf.id == ProductId)
		{
			// Capacitive touch is present, configure the chip
			//status = WriteRead(AltConfigurationData, sizeof AltConfigurationData);
			status = WriteRead(ConfigurationData, sizeof ConfigurationData);
			m_tmr.Start();
			if (IsStatusOk(status))
				return true;
		}
		
		// Revert PA19 to RTP MISO
		SetPinConfigA(PORT_PINCFG_INEN | PORT_PINCFG_PMUXEN, CtpReset_BIT);
		Disable();
		return false;
	}
	
	bool Process()
	{
		uint	status;
		
		status = GetStatus();
		if (IsBusy(status))
			return false;
		
		switch (m_i2cState)
		{
		case I2CSTATE_GetData:
			m_i2cState = I2CSTATE_Ready;
			if (!IsStatusOk(status))
				return false;
				
			// Touch data returned
			if (m_inBuf.touch.bufStatus)
			{
				StartWriteRead(BufReset, sizeof BufReset);
				m_i2cState = I2CSTATE_ResetFlag;
				
				if (m_inBuf.touch.touchCount != 0)
				{
					// Touching
					ProcessRaw(m_inBuf.touch.Coordinates[0].X, m_inBuf.touch.Coordinates[0].Y);
					IsTouched(true);
					return true;
				}
			}
		
			// Not touching
			IsTouched(false);
			return true;
		
		case I2CSTATE_ResetFlag:
			m_i2cState = I2CSTATE_Ready;
			break;
		
		default:
			// I2C was not busy
			if (m_tmr.CheckInterval_rate(TouchUpdateRate))
			{
				StartWriteRead(TouchRegister, sizeof TouchRegister, &m_inBuf.touch, sizeof m_inBuf.touch);
				m_i2cState = I2CSTATE_GetData;
			}
			break;
		}
		return false;
	}
	
	bool CheckLeaveStandby()
	{
		if (GetTouchIrq() == 0)
		{
			WriteRead(TouchRegister, sizeof TouchRegister, m_inBuf.idBytes, 1);
			if (m_inBuf.touch.bufStatus)
			{
				WriteRead(BufReset, sizeof BufReset);
				if (m_inBuf.touch.touchCount != 0)
					return true;
			}
		}
		return false;
	}

protected:
	void StartWriteRead(const void *pWrite, int cbWrite, void *pRead = NULL, int cbRead = 0) NO_INLINE_ATTR
	{
		I2c::StartWriteRead(I2cAddr, pWrite, cbWrite, pRead, cbRead);
	}
	
	uint WriteRead(const void *pWrite, int cbWrite, void *pRead = NULL, int cbRead = 0) NO_INLINE_ATTR
	{
		uint	status;
		
		StartWriteRead(pWrite, cbWrite, pRead, cbRead);
		while (IsBusy(status = GetStatus()));
		return status;
	}
	
	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const byte IdRegister[] = { REGISTER(REG_ProdId0) };
	inline static const byte TouchRegister[] = { REGISTER(REG_TouchCoords) };
	inline static const byte BufReset[] = { REGISTER(REG_TouchCoords), 0 };

	// BuyDisplay.com has code samples with different values. 
	// Have no idea what they mean, but both seem to work.
	inline static const byte ConfigurationData[]=
	{
		REGISTER(REG_ConfigStart),
		0x63,0x00,0x04,0x58,0x02,0x0A,0x3D,0x00,
		0x01,0x08,0x28,0x0F,0x50,0x32,0x03,0x05,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x17,
		0x19,0x1D,0x14,0x90,0x2F,0x89,0x23,0x25,
		0xD3,0x07,0x00,0x00,0x00,0x02,0x03,0x1D,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x19,0x32,0x94,0xD5,0x02,
		0x07,0x00,0x00,0x04,0xA2,0x1A,0x00,0x90,
		0x1E,0x00,0x80,0x23,0x00,0x73,0x28,0x00,
		0x68,0x2E,0x00,0x68,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x16,0x15,0x14,0x11,0x10,0x0F,0x0E,0x0D,
		0x0C,0x09,0x08,0x07,0x06,0x05,0x04,0x01,
		0x00,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x29,0x28,
		0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20,
		0x1F,0x1E,0x1C,0x1B,0x19,0x14,0x13,0x12,
		0x11,0x10,0x0F,0x0E,0x0D,0x0C,0x0A,0x08,
		0x07,0x06,0x04,0x02,0x00,0xFF,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x71,0x01
	};

	inline static const byte AltConfigurationData[]=
	{
		REGISTER(REG_ConfigStart),
		0x5A,0x00,0x04,0x58,0x02,0x0A,0x0D,0x00,
		0x01,0x0A,0x28,0x0F,0x50,0x32,0x03,0x05,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x8E,0x2E,0x88,0x3C,0x3E,
		0xD3,0x07,0x00,0x00,0x01,0x00,0x02,0x2D,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x32,0x7D,0x94,0xC5,0x02,
		0x07,0x00,0x00,0x04,0x80,0x37,0x00,0x75,
		0x42,0x00,0x64,0x4F,0x00,0x56,0x5F,0x00,
		0x4B,0x72,0x00,0x4B,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x15,0x14,0x11,0x10,0x0F,0x0E,0x0D,0x0C,
		0x09,0x08,0x07,0x06,0x05,0x04,0x01,0x00,
		0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,
		0x04,0x06,0x07,0x08,0x0A,0x0C,0x0D,0x0F,
		0x10,0x11,0x12,0x13,0x19,0x1B,0x1C,0x1E,
		0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
		0x27,0x28,0xFF,0xFF,0xFF,0xFF,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xA6,0x01 
	};

	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	uint	m_i2cState;
	union
	{
		 byte	idBytes[4];
		 uint	id;
		 TouchCoords	touch;
	} m_inBuf;
};

extern Gt9271 CapTouch;
