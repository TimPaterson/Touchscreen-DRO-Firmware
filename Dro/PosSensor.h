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
	PosSensor(SensorInfo *pInfo) : m_pInfo{pInfo}	{}

public:
	// Called from ISR
	void InputChange(uint uSignal) INLINE_ATTR
	{
		uSignal &= 3;	// low two bits - A and B signals
		m_posCur += s_arbQuadDecode[(m_bPrevSig << 2) + uSignal];
		m_bPrevSig = uSignal;
	}

public:
	uint GetResolution()				{ return m_pInfo->Resolution; }
	bool GetDirection()					{ return m_pInfo->Direction; }
	bool IsDisabled()					{ return m_pInfo->Disable; }
	void SetDisable(bool fDis)			{ m_pInfo->Disable = fDis; }
	double GetCorrectionPpm()			{ return (m_pInfo->Correction - 1.0) * 1E6; }
	long GetRelativePos()				{ return m_posCur; }
	long GetOrigin(uint i)				{ return m_arOrigins[i]; }
	void SetOrigin(uint i, long pos)	{ m_arOrigins[i] = pos; }
	void AdjustOrigin(uint i, long pos)	{ m_arOrigins[i] += pos; }
	void AdjustOrigin(long pos)			{ AdjustOrigin(Eeprom.Data.OriginNum, pos); }
		
public:
	static int GetDecimals()			{ return IsMetric() ? 3 : 4; }

public:
	double GetPosition()
	{
		long	pos;

		pos = m_arOrigins[Eeprom.Data.OriginNum] + m_posCur;
		if (IsMetric())
			return nearbyint(pos * m_scaleMm + m_offsetMm) / m_mmRounding;

		return nearbyint(pos * m_scaleInch + m_offsetInch) / m_inchRounding;
	}

	double GetDistance()
	{
		long	pos, delta;

		pos = m_posCur;
		delta = m_posLast - pos;
		m_posLast = pos;

		return GetDistance(delta);
	}

	double GetDistance(int delta)
	{
		if (IsMetric())
			return nearbyint(delta * m_scaleMm) / m_mmRounding;

		return nearbyint(delta * m_scaleInch) / m_inchRounding;
	}

	long SetPosition(double pos)
	{
		long	posNew, posOld;

		posNew = ConvertPosToInt(pos);
		posOld = m_arOrigins[Eeprom.Data.OriginNum];
		m_arOrigins[Eeprom.Data.OriginNum] = posNew;
		return posOld - posNew;
	}

	void SetOffset(double offset)
	{
		// Offset is in current units
		if (IsMetric())
		{
			m_offsetMm = offset * m_mmRounding;
			m_offsetInch = offset / MmPerInch * m_inchRounding;
		}
		else
		{
			m_offsetInch = offset * m_inchRounding;
			m_offsetMm = offset * MmPerInch * m_mmRounding;
		}
	}

	bool SetCorrectionPpm(double pos)
	{
		if (fabs(pos) >= MaxCompensationPpm)
			return false;

		m_pInfo->Correction = pos / 1E6 + 1.0;
		SensorInfoUpdate();
		return true;
	}

	void SetResolution(uint res)
	{
		m_pInfo->Resolution = res;
		SensorInfoUpdate();
	}

	void SetDirection(bool dir)
	{
		m_pInfo->Direction = dir;
		SensorInfoUpdate();
	}

	void SensorInfoUpdate()
	{
		// Set up display rounding according to sensor resolution
		// For 1, 2, 5, 10 um
		if (m_pInfo->Resolution > 5)
		{
			m_inchRounding = 2000.0;	// 1/2000 in = 0.0005
			m_mmRounding = 100.0;		// 1/100 mm = 0.01
		}
		else if (m_pInfo->Resolution > 2)
		{
			m_inchRounding = 5000.0;	// 1/5000 in = 0.0002
			m_mmRounding = 200.0;		// 1/200 mm = 0.005
		}
		else if (m_pInfo->Resolution > 1)
		{
			m_inchRounding = 10000.0;	// 1/10000 in = 0.0001
			m_mmRounding = 500.0;		// 1/500 mm = 0.002
		}
		else
		{
			m_inchRounding = 20000.0;	// 1/20000 in = 0.00005
			m_mmRounding = 1000.0;		// 1/500 mm = 0.001
		}

		m_scaleMm = m_pInfo->Correction * m_pInfo->Resolution * m_mmRounding / 1000.0;
		if (m_pInfo->Direction)
			m_scaleMm = -m_scaleMm;
		m_scaleInch = m_scaleMm * m_inchRounding / m_mmRounding / MmPerInch;
	}

public:
	static bool IsMetric()
	{
		return Eeprom.Data.fIsMetric;
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	long ConvertPosToInt(double pos)
	{
		// Round to display value first
		if (IsMetric())
		{
			pos = nearbyint(pos * m_mmRounding - m_offsetMm);
			pos /= m_scaleMm;
		}
		else
		{
			pos = nearbyint(pos * m_inchRounding - m_offsetInch);
			pos /= m_scaleInch;
		}

		return lround(pos) - m_posCur;
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
	volatile byte m_bPrevSig;
	volatile long m_posCur;

private:
	SensorInfo	*m_pInfo;
	long		m_posLast;
	double		m_scaleMm;
	double		m_scaleInch;
	double		m_offsetMm;
	double		m_offsetInch;
	double		m_inchRounding;
	double		m_mmRounding;
	long		m_arOrigins[MaxOrigins];
};
