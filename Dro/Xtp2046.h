//****************************************************************************
// Class Xtp2046
// Xtp2046.h
//
// Created 10/13/2020 4:35:49 PM by Tim
//
//****************************************************************************

#pragma once

#include <Com/Spi.h>
#include "TouchMgr.h"


//****************************************************************************
// Default EEPROM values

static constexpr ushort TouchDefaultMinZ = 200;
static constexpr int TouchUpdateRate = 30;	// full touch scans per second
static constexpr int TouchInitialDiscard = 4;
static constexpr int TouchAverageShift = 3;


class Xtp2046 : public TouchMgr, DECLARE_SPI(SERCOM1, RtpCs_PIN)
{
protected:
	static constexpr int BaudRate = 1'000'000;
	static constexpr int Resolution = 4096;
	static constexpr double Reference = 3.3;
	// Temp reference diode is 0.6V at 25C. Allow 0.4V - 0.8V.
	static constexpr int MinTempReading = 0.4 * Resolution / Reference;
	static constexpr int MaxTempReading = 0.8 * Resolution / Reference;

protected:
	// Types
	enum ControlByte
	{
		// Analog input selections
		// These are for differential mode
		ADDR_X_Val = 5,
		ADDR_Y_Val = 1,
		ADDR_Z1_Val = 3,
		ADDR_Z2_Val = 4,
		ADDR_Temp0_Val = 0,
		ADDR_Temp1_Val = 7,
		

		ADDR_pos = 4,	// bit position in control byte

		ADDR_X =	(ADDR_X_Val << ADDR_pos),
		ADDR_Y =	(ADDR_Y_Val << ADDR_pos),
		ADDR_Z1 =	(ADDR_Z1_Val << ADDR_pos),
		ADDR_Z2 =	(ADDR_Z2_Val << ADDR_pos),
		ADDR_Temp0 = (ADDR_Temp0_Val << ADDR_pos),
		ADDR_Temp1 = (ADDR_Temp1_Val << ADDR_pos),

		// Mode: 12-bit or 8-bit
		MODE_12Bit_Val = 0,
		MODE_8Bit_Val = 1,

		MODE_pos = 3,

		MODE_12Bit =	(MODE_12Bit_Val << MODE_pos),
		MODE_8Bit =		(MODE_8Bit_Val << MODE_pos),

		// Reference: single ended or differential
		REF_Dif_Val = 0,
		REF_Sngl_Val = 1,

		REF_pos = 2,

		REF_Dif =		(REF_Dif_Val << REF_pos),
		REF_Sngl =		(REF_Sngl_Val  << REF_pos),

		// Power down mode
		PWR_Save_Val = 0,
		PWR_RefOffAdcOn_Val = 1,
		PWR_RefOnAdcOff_Val = 2,
		PWR_On_Val = 3,

		PWR_pos = 0,

		PWR_Save =			(PWR_Save_Val << PWR_pos),
		PWR_RefOffAdcOn =	(PWR_RefOffAdcOn_Val << PWR_pos),
		PWR_RefOnAdcOff =	(PWR_RefOnAdcOff_Val << PWR_pos),
		PWR_On =			(PWR_On_Val << PWR_pos),

		// Start bit
		RTP_Start = 0x80,

		// Combined values
		RTP_ReadX = RTP_Start | MODE_12Bit | REF_Dif | PWR_Save | ADDR_X,
		RTP_ReadY = RTP_Start | MODE_12Bit | REF_Dif | PWR_Save | ADDR_Y,
		RTP_ReadZ1 = RTP_Start | MODE_12Bit | REF_Dif | PWR_Save | ADDR_Z1,
		RTP_ReadZ2 = RTP_Start | MODE_12Bit | REF_Dif | PWR_Save | ADDR_Z2,
		RTP_ReadTemp0 = RTP_Start | MODE_12Bit | REF_Sngl | PWR_Save | ADDR_Temp0,
		RTP_ReadTemp1 = RTP_Start | MODE_12Bit | REF_Sngl | PWR_Save | ADDR_Temp1,
	};


public:
	bool Init(SpiInPad padMiso, SpiOutPad padMosi)
	{
		SpiInit(padMiso, padMosi, SPIMODE_0);
		SetBaudRateConst(BaudRate);

		m_minZtouch = TouchDefaultMinZ;
		m_avgShift = TouchAverageShift;
		m_discardCnt = TouchInitialDiscard;
		m_sampleCnt = (1 << m_avgShift) + m_discardCnt;
		m_scanTicks = Timer::TicksFromFreq(TouchUpdateRate * m_sampleCnt);
		
		Enable();
		
		// Measure temperature to see if chip is present
		int temp = ReadValue(RTP_ReadTemp0);
		if (temp >= MinTempReading && temp <= MaxTempReading)
		{
			m_tmr.Start();
			return true;
		}
		
		Disable();
		return false;
	}

	bool Process()
	{
		if (!m_tmr.CheckInterval_ticks(m_scanTicks))
			return false;

		//if (GetTouchIrq() == 0)
		ushort rawZ = ReadValue(RTP_ReadZ1);
		if (rawZ >= m_minZtouch)
		{
			// Touching
			if (!m_fPrevTouch)
			{
				// Just starting contact
				m_sumX = 0;
				m_sumY = 0;
				m_cAvg = 0;
			}
			m_fPrevTouch = true;
			if (++m_cAvg <= m_discardCnt)
				return false;

			m_sumX += ReadValue(RTP_ReadX);
			m_sumY += ReadValue(RTP_ReadY);

			if (m_cAvg < m_sampleCnt)
				return false;

			ProcessRaw(m_sumX >> m_avgShift, m_sumY >> m_avgShift);
			IsTouched(true);
			m_sumX = 0;
			m_sumY = 0;
			m_cAvg = 0;
		}
		else
		{
			// Not touching
			if (m_fPrevTouch)
				m_cAvg = 0;	// Just ending contact

			m_fPrevTouch = false;
			if (++m_cAvg < m_sampleCnt)
				return false;

			IsTouched(false);
			m_cAvg = 0;
		}

		return true;
	}
	
	bool CheckLeaveStandby()
	{
		return GetTouchIrq() == 0;
	}

protected:
	static uint ReadValue(byte bControl) NO_INLINE_ATTR
	{
		uint	val;

		Select();
		// Read twice to get stable values
		WriteByte(bControl);
		WriteByte();
		WriteByte(bControl);
		val = WriteByte() << 5;
		val |= WriteByte() >> 3;
		Deselect();
		return val;
	}

protected:
	ushort	m_sumX;
	ushort	m_sumY;

	ushort	m_minZtouch;
	ushort	m_scanTicks;
	byte	m_avgShift;
	byte	m_sampleCnt;
	byte	m_discardCnt;
	byte	m_cAvg;
	bool	m_fPrevTouch;
};
