//****************************************************************************
// ProgressBar.h
//
// Created 3/13/2021 12:45:12 PM by Tim
//
//****************************************************************************

#pragma once

#include "ScreenMgr.h"


class ProgressBar : ScreenMgr
{
	static constexpr int ProgressShift = __builtin_clz(ScreenWidth - 1);
	static constexpr uint ProgressScale = 1 << ProgressShift;

public:
	ProgressBar(Canvas &canvas, const Area &area, ulong foreColor, ulong backColor) :
		m_pCanvas{&canvas},	m_foreColor{foreColor}, m_backColor{backColor}
		{
			SetArea(area);
		}

	//*********************************************************************
	// Public interface
	//*********************************************************************
public:
	void SetArea(const Area &area)
	{
		m_pArea = &area;
		m_fVertical = area.Height > area.Width;
	}

	void SetMax(ulong max)
	{
		m_factor = DivUintRnd((m_fVertical ? m_pArea->Height : m_pArea->Width) << ProgressShift, max);
	}

	void IncreaseValue(ulong value)
	{
		SetValue(m_lastValue + value);
	}
	
	void SetValue(ulong value)
	{
		Area	area;
		
		if (value == 0)
		{
			FillRect(m_pCanvas, m_pArea, m_backColor);
			m_lastValue = 0;
			m_lastPos = 0;
			return;
		}
		m_lastValue = value;

		value = (value * m_factor) >> ProgressShift;
		if (value <= m_lastPos)
			return;
			
		area = *m_pArea;
		if (m_fVertical)
		{
			if (value > area.Height)
				value = area.Height;
			area.Ypos += value;
			area.Height = value;
			FillRect(m_pCanvas, &area, m_foreColor);
		}
		else
		{
			if (value > area.Width)
				value = area.Width;
			area.Width = value;
			FillRect(m_pCanvas, &area, m_foreColor);
		}
		m_lastPos = value;
	}

	//*********************************************************************
	// instance data
	//*********************************************************************
protected:
	bool		m_fVertical;
	ushort		m_lastPos;
	ulong		m_lastValue;
	Canvas		*m_pCanvas;
	const Area	*m_pArea;
	ulong		m_foreColor;
	ulong		m_backColor;
	ulong		m_factor;
};
