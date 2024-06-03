//****************************************************************************
// Class PosSensor
// PosSensor.h
//
// Created 10/13/2020 12:57:40 PM by Tim
//
//****************************************************************************

#pragma once


class PosSensor
{
public:
	static constexpr int MaxOrigins = 2;

protected:
	static constexpr double MaxCompensationPpm = 1000.0;	// max adjust of 0.1%

	//*********************************************************************
	// Public interface
	//*********************************************************************3
public:
	PosSensor(SensorInfo *pInfo) : m_pInfo{pInfo} 	{}

public:
	// Called from ISR
	void InputChange(uint uSignal) INLINE_ATTR
	{
		uSignal &= 3;	// low two bits - A and B signals
		v_posCur += s_arbQuadDecode[(v_bPrevSig << 2) + uSignal] * m_pInfo->Resolution;
		v_bPrevSig = uSignal;
	}

public:
	uint GetSensorResolution()			{ return m_pInfo->Resolution; }
	bool GetDirection()					{ return m_pInfo->Direction; }
	bool IsDisabled()					{ return m_pInfo->Disable; }
	void SetDisable(bool fDis)			{ m_pInfo->Disable = fDis; }
	double GetCorrectionPpm()			{ return (m_pInfo->Correction - 1.0) * 1E6; }
	long GetRelativePos()				{ return v_posCur; }
	long GetOrigin(uint i)				{ return m_arOrigins[i]; }
	void SetOrigin(uint i, long pos)	{ m_arOrigins[i] = pos; }
	void AdjustOrigin(long pos)			{ m_arOrigins[Eeprom.Data.OriginNum] += pos; }
		
public:
	double GetSensorPosition() NO_INLINE_ATTR
	{
		// returns mm, unrounded
		return (m_arOrigins[Eeprom.Data.OriginNum] + v_posCur) * m_scale;
	}
	
	double GetAbsPosition()
	{
		// independent of origin
		return v_posCur * m_scale;
	}

	double GetSensorDistance()
	{
		long	pos, delta;

		pos = v_posCur;
		delta = m_posLast - pos;
		m_posLast = pos;

		return GetSensorDistance(delta);
	}

	double GetSensorDistance(long delta)
	{
		return delta * m_scale;
	}

	long SetSensorPosition(double pos)
	{
		long	posOld;
		long	posNew;

		posNew = lround(pos / m_scale) - v_posCur;
		posOld = m_arOrigins[Eeprom.Data.OriginNum];
		m_arOrigins[Eeprom.Data.OriginNum] = posNew;
		return posOld - posNew;
	}
	
	void AdjustSensorPosition(double adjust)
	{
		long counts = lround(adjust / m_scale);
		__disable_irq();
		v_posCur += counts;
		__enable_irq();
	}
	

	bool SetCorrectionPpm(double pos)
	{
		if (fabs(pos) >= MaxCompensationPpm)
			return false;

		m_pInfo->Correction = pos / 1E6 + 1.0;
		SensorInfoUpdate();
		return true;
	}

	void SetSensorResolution(uint res)
	{
		m_pInfo->Resolution = res;
	}

	void SetDirection(bool dir)
	{
		m_pInfo->Direction = dir;
		SensorInfoUpdate();
	}

	void SensorInfoUpdate()
	{
		m_scale = m_pInfo->Correction / UnitFactor;
		if (m_pInfo->Direction)
			m_scale = -m_scale;
	}

public:
	static bool IsMetric()
	{
		return Eeprom.Data.fIsMetric;
	}

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
private:
	inline static const sbyte s_arbQuadDecode[16]
	{
	//			Bprev	Aprev	Bcur	Acur
		 0, //	 0		 0		 0		 0		no change
		+1, //	 0		 0		 0		 1		A rise, B lo
		-1, //	 0		 0		 1		 0		B rise, A lo
		 0, //	 0		 0		 1		 1		both change, invalid
		-1, //	 0		 1		 0		 0		A fall, B lo
		 0, //	 0		 1		 0		 1		no change
		 0, //	 0		 1		 1		 0		both change, invalid
		+1, //	 0		 1		 1		 1		B rise, A hi
		+1, //	 1		 0		 0		 0		B fall, A lo
		 0, //	 1		 0		 0		 1		both change, invalid
		 0, //	 1		 0		 1		 0		no change
		-1, //	 1		 0		 1		 1		A rise, B hi
		 0, //	 1		 1		 0		 0		both change, invalid
		-1, //	 1		 1		 0		 1		B fall, A hi
		+1, //	 1		 1		 1		 0		A fall, B hi
		 0, //	 1		 1		 1		 1		no change
	};

	//*********************************************************************
	// member (RAM) data
	//*********************************************************************
private:
	// can change in ISR
	volatile long v_posCur;
	volatile byte v_bPrevSig;

private:
	SensorInfo	*m_pInfo;
	long		m_posLast;
	double		m_scale;
	long		m_arOrigins[MaxOrigins];
};
