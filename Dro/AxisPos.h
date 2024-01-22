//****************************************************************************
// AxisPos.h
//
// Created 11/20/2021 12:56:17 PM by Tim
//
//****************************************************************************

#pragma once

#include "PosSensor.h"


class AxisPos : public PosSensor
{
	static constexpr int UndoLevels = 8;	// Should be power of 2
	static constexpr double MaxResolution = 20.0;	// microns
	static constexpr double MinResolution = 0.1;	// microns

	struct UndoInfo
	{
		ushort	count;
		ushort	cur;
		long	value[UndoLevels];
	};

	//*********************************************************************3
	// Constructor
public:
	AxisPos(SensorInfo *pInfoZ, PosSensor *pSense = NULL) : 
		PosSensor(pInfoZ), m_pSenseQ(pSense) {}

public:
	void SetSensor(PosSensor* pSense)	{ m_pSenseQ = pSense; }
			
public:
	static int GetDecimals()			{ return IsMetric() ? 3 : 4; }

public:
	double GetPosition() NO_INLINE_ATTR
	{
		double	pos;

		pos = PosSensor::GetPosition() + m_offset;

		if (m_useFactor)
			pos += m_pSenseQ->GetPosition() * m_qFactor;

		if (IsMetric())
			return round(pos * m_mmRounding) / m_mmRounding;
		
		return round(pos * m_inchRounding / MmPerInch) / m_inchRounding;
	}

	long SetPosition(double pos)
	{
		long		posUndo;
		UndoInfo	&undo = m_arUndoInfo[Eeprom.Data.OriginNum];

		if (!IsMetric())
			pos *= MmPerInch;
			
		if (m_useFactor)
			pos -= m_pSenseQ->GetPosition() * m_qFactor;

		posUndo = PosSensor::SetPosition(pos - m_offset);
		if (posUndo != 0)
		{
			++undo.cur %= UndoLevels;
			undo.value[undo.cur] = posUndo;
			if (undo.count < UndoLevels)
				undo.count++;
		}
		return posUndo;
	}
	
	bool Undo()
	{
		UndoInfo	&undo = m_arUndoInfo[Eeprom.Data.OriginNum];

		if (undo.count == 0)
			return false;

		AdjustOrigin(undo.value[undo.cur]);
		undo.value[undo.cur] = 0;
		if (--undo.cur < 0)
			undo.cur = UndoLevels;
		undo.count--;
		return true;
	}
	
	double GetUndoValue(int level)
	{
		UndoInfo	&undo = m_arUndoInfo[Eeprom.Data.OriginNum];
		double		dist;
		
		level = undo.cur - level;
		if (level < 0)
			level += UndoLevels;
			
		dist = PosSensor::GetDistance(undo.value[level]);
		
		if (!IsMetric())
			dist /= MmPerInch;
			
		return dist;
	}
	
	double GetDistance()
	{
		double delta;
		
		delta = PosSensor::GetDistance();
		
		if (m_useFactor)
			delta += m_pSenseQ->GetDistance();
			
		if (!IsMetric())
			delta /= MmPerInch;
			
		return delta;
	}
	
	void SetOffset(long offset)
	{
		m_offset = (double)offset / UnitFactor;
	}
	
	void SetFactor(double factor)
	{
		m_qFactor = factor;
		m_useFactor = m_pSenseQ != NULL && !m_pSenseQ->IsDisabled() && factor != 0;
		SetRounding();
	}

	double GetResolution()	
	{ 
		return (double)PosSensor::GetResolution() * 1000 / UnitFactor; 
	}
	
	void SetResolution(double res)
	{
		if (res > MaxResolution || res < MinResolution)
			return;
			
		PosSensor::SetResolution(lround(res * (UnitFactor / 1000)));	// per micron instead of per mm
		SetRounding();
	}
	
	void SetRounding()
	{
		uint	res;
		
		res = GetResolution();
		if (m_useFactor)
			res = std::min(res, m_pSenseQ->GetResolution());
			
		// Set up display rounding according to sensor resolution
		// For 1, 2, 5, 10 um. Resolution in 0.1um units
		if (res > 50)
		{
			m_inchRounding = 2000.0;	// 1/2000 in = 0.0005
			m_mmRounding = 100.0;		// 1/100 mm = 0.01
		}
		else if (res > 20)
		{
			m_inchRounding = 5000.0;	// 1/5000 in = 0.0002
			m_mmRounding = 200.0;		// 1/200 mm = 0.005
		}
		else if (res > 10)
		{
			m_inchRounding = 10000.0;	// 1/10000 in = 0.0001
			m_mmRounding = 500.0;		// 1/500 mm = 0.002
		}
		else
		{
			m_inchRounding = 20000.0;	// 1/20000 in = 0.00005
			m_mmRounding = 1000.0;		// 1/500 mm = 0.001
		}
	}

	long GetSavePos()
	{
		return GetRelativePos();
	}

	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	bool		m_useFactor;
	PosSensor	*m_pSenseQ;
	double		m_qFactor;
	double		m_offset;
	double		m_inchRounding;
	double		m_mmRounding;
	UndoInfo	m_arUndoInfo[PosSensor::MaxOrigins];
};

