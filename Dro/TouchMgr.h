//****************************************************************************
// TouchMgr.h
//
// Created 10/26/2020 5:21:30 PM by Tim
//
//****************************************************************************

#pragma once


struct ScaleMatrix
{
	int		aScale;
	int		bScale;
	int		base;
};

struct TouchInfo
{
	ScaleMatrix	scaleX;
	ScaleMatrix	scaleY;
	ushort	minZtouch;
	byte	updateRate;
	byte	sampleDiscard;
	byte	averageShift;
	byte	reserved[3];	// round up to multiple of 32 bits
};

enum TouchFlags
{
	TOUCH_None = 0,
	TOUCH_Start = 1,
	TOUCH_End = 2,
	TOUCH_Touched = 4,
};

static constexpr double StandbyDebounceTimeMs = 100.0;
static constexpr int TouchShift = 18;
static constexpr int TouchScale = 1 << TouchShift;


class TouchMgr
{
	// Types
protected:
	class Position
	{
	public:
		void Set(int posA, int posB)
		{
			posA = posA * m_pScale->aScale + posB *m_pScale->bScale;
			posA = ShiftIntRnd(posA, TouchShift) + m_pScale->base;
			if (posA < 0)
				posA = 0;
			else if (posA > m_max)
				posA = m_max;
			m_cur = posA;
		}

		uint Get()									{ return m_cur; }
		void SetMax(uint max)						{ m_max = max; }
		void SetMatrix(const ScaleMatrix &scale)	{ m_pScale = &scale; }

	protected:
		const ScaleMatrix	*m_pScale;
		ushort		m_max;
		ushort		m_cur;
	};

public:
	static uint GetX()		{ return m_posX.Get(); }
	static uint GetY()		{ return m_posY.Get(); }
	static ushort GetRawX()	{ return m_rawX; }
	static ushort GetRawY()	{ return m_rawY; }
		
public:
	static uint GetTouch()
	{
		uint flags = m_touchFlags;
		m_touchFlags &= ~(TOUCH_Start | TOUCH_End);	//  clear edge triggered events
		return flags;
	}

	static void SetSize(uint maxX, uint maxY)
	{
		m_posX.SetMax(maxX - 1);
		m_posY.SetMax(maxY - 1);
	}

	static void SetMatrix(TouchInfo *pInfo)
	{
		m_posX.SetMatrix(pInfo->scaleX);
		m_posY.SetMatrix(pInfo->scaleY);
	}

public:
	// This blocks waiting for no touch. Used to enter/exit standby mode.
	void WaitTouchRelease()
	{
		Timer	timer;
		
		timer.Start();
		for (;;)
		{
			while (!Process());
			if (GetTouch() & TOUCH_Touched)
			{
				timer.Start();
				continue;
			}
			
			if (timer.CheckInterval_ms(StandbyDebounceTimeMs))
				return;
		}
	}

public:
	virtual bool Process() { return false; }
	virtual bool CheckLeaveStandby() { return true; }
	
protected:
	static void ProcessRaw(ushort rawX, ushort rawY)
	{
		m_rawX = rawX;
		m_rawY = rawY;
		m_posX.Set(rawX, rawY);
		m_posY.Set(rawY, rawX);
	}

	static void IsTouched(bool fIsTouched)
	{
		uint	flags;

		flags = m_touchFlags;
		if (fIsTouched)
		{
			if (!(flags & TOUCH_Touched))
				flags = TOUCH_Start | TOUCH_Touched;
			else
				flags = TOUCH_Touched;
		}
		else
		{
			if (flags & TOUCH_Touched)
				flags = TOUCH_End;
			else
				flags = TOUCH_None;
		}
		m_touchFlags = flags;
	}
	
	//*********************************************************************
	// static (RAM) data
	//
	// Data is all static because there's only one touch panel
	//*********************************************************************
protected:
	inline static Timer		m_tmr;
private:
	inline static byte		m_touchFlags;
	inline static ushort	m_rawX;
	inline static ushort	m_rawY;
	inline static Position	m_posX;
	inline static Position	m_posY;
};

extern TouchMgr*	pTouch;
