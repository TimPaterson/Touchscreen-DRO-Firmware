//****************************************************************************
// Drawing.h
//
// Created 1/16/2024 12:52:56 PM by Tim
//
//****************************************************************************

#pragma once

#include "ScreenMgr.h"


struct Rectangle
{
	float	x1;
	float	y1;
	float	x2;
	float	y2;
	ulong	color;
};


class RotateRectangle
{
public:
	RotateRectangle(ushort x, ushort y) : m_centerX(x), m_centerY(y) {}
		
public:
	void SetCenter(ushort x, ushort y)
	{
		m_centerX = x;
		m_centerY = y;
	}
	
	void SetAngle(double angle)
	{
		double	sine, cosine;
		
		__builtin_sincos(angle * M_PI / 180, &sine, &cosine);
		m_rotSin = (float)sine;
		m_rotCos = (float)cosine;
	}
	
	void DrawList(const Rectangle* pRect, int count)
	{
		int	x, y;
		
		for (; count > 0; count--, pRect++)
		{
			x = lroundf(pRect->x1 * m_rotCos - pRect->y1 * m_rotSin) + m_centerX;
			y = lroundf(pRect->x1 * m_rotSin + pRect->y1 * m_rotCos) + m_centerY;
			Lcd.WriteRegXY(DLHSR0, x, y);
			
			x = lroundf(pRect->x2 * m_rotCos - pRect->y2 * m_rotSin) + m_centerX;
			y = lroundf(pRect->x2 * m_rotSin + pRect->y2 * m_rotCos) + m_centerY;
			Lcd.WriteRegXY(DLHER0, x, y);
			
			Lcd.SetForeColor(pRect->color);
			
			// Draw first triangle
			x = lroundf(pRect->x2 * m_rotCos - pRect->y1 * m_rotSin) + m_centerX;
			y = lroundf(pRect->x2 * m_rotSin + pRect->y1 * m_rotCos) + m_centerY;
			Lcd.WriteRegXY(DTPH0, x, y);			
			Lcd.WriteReg(DCR0, DCR0_DrawTriangle | DCR0_FillOn | DCR0_DrawActive);
			Lcd.WaitWhileBusy();
			
			// Draw second triangle
			x = lroundf(pRect->x1 * m_rotCos - pRect->y2 * m_rotSin) + m_centerX;
			y = lroundf(pRect->x1 * m_rotSin + pRect->y2 * m_rotCos) + m_centerY;
			Lcd.WriteRegXY(DTPH0, x, y);
			Lcd.WriteReg(DCR0, DCR0_DrawTriangle | DCR0_FillOn | DCR0_DrawActive);
			Lcd.WaitWhileBusy();
		}
	}
	
	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	ushort	m_centerX;
	ushort	m_centerY;
	float	m_rotSin;
	float	m_rotCos;
};
