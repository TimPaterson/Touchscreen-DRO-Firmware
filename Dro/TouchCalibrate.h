//****************************************************************************
// TouchCalibrate.h
//
// Created 3/5/2021 3:00:10 PM by Tim
//
//****************************************************************************

#pragma once

#include "ScreenMgr.h"


class TouchCalibrate_t : TouchCanvas, ScreenMgr
{
	// Calibration target position
	static constexpr int TouchEdgeOffsetX = ScreenWidth / 10;
	static constexpr int TouchEdgeOffsetY = ScreenHeight / 10;
	static constexpr int RightTarget = ScreenWidth - TouchEdgeOffsetX - 1;
	static constexpr int BottomTarget = ScreenHeight - TouchEdgeOffsetY - 1;
	static constexpr int MiddleTargetX = ScreenWidth / 2;
	static constexpr int MiddleTargetY = ScreenHeight / 2;

	// Calibration
	static constexpr int AbortFlag = -1;
	static constexpr int TouchTimeMs = 500;

	// Colors
	static constexpr ulong TargetColor = 0;
	static constexpr ulong ButtonColor = 0x0000FF;
	static constexpr ulong ButtonTextColor = 0xFFFFFF;
	static constexpr ulong TextColor = 0;
	static constexpr ulong BackColor = ScreenBackColor;

	// Text messages
	#define REPEAT_BTN_LABEL	"Calibrate"
	#define DONE_BTN_LABEL		"Accept"
	#define SKIP_BTN_LABEL		"Exit"
	#define RESTART_LABEL		"Cycle power to restart calibration"
	#define VERIFY_LABEL		"Touch anywhere to verify accuracy"
	// 16 x 32 internal font is used
	static constexpr int CharHeight = 32;
	static constexpr int CharWidth = 16;
	// x2 multiplier on buttons
	static constexpr int ButtonCharHeight = CharHeight * 2;
	static constexpr int ButtonCharWidth = CharWidth * 2;
	// Define the a big button to repeat
	static constexpr int RepeatBtnHeight = ScreenHeight / 5;
	static constexpr int RepeatBtnWidth = ScreenWidth / 3;
	static constexpr int RepeatBtnLeft = RepeatBtnWidth;
	static constexpr int RepeatBtnTop = 0;
	// Define the button quit
	static constexpr int DoneBtnHeight = RepeatBtnHeight / 2;
	static constexpr int DoneBtnWidth = (STRLEN(DONE_BTN_LABEL) + 2) * ButtonCharWidth;
	static constexpr int DoneBtnLeft = (ScreenWidth - DoneBtnWidth) / 2;
	static constexpr int DoneBtnTop = ScreenHeight - DoneBtnHeight;
	// Positions within button
	static constexpr int ReapeatTextTop = (RepeatBtnHeight - ButtonCharHeight) / 2;
	static constexpr int RepeatTextLeft = (RepeatBtnWidth - STRLEN(REPEAT_BTN_LABEL) * ButtonCharWidth) / 2;
	static constexpr int DoneTextTop = (DoneBtnHeight - ButtonCharHeight) / 2;
	static constexpr int DoneTextLeft = (DoneBtnWidth - STRLEN(DONE_BTN_LABEL) * ButtonCharWidth) / 2;
	static constexpr int SkipTextLeft = (DoneBtnWidth - STRLEN(SKIP_BTN_LABEL) * ButtonCharWidth) / 2;
	// Position on screen
	static constexpr int TextVertMargin = 6;
	static constexpr int RestartTextTop = DoneBtnTop - CharHeight - TextVertMargin;
	static constexpr int RestartTextLeft = (ScreenWidth - STRLEN(RESTART_LABEL) * CharWidth) / 2;
	static constexpr int VerifyTextTop = RepeatBtnTop + RepeatBtnHeight + TextVertMargin;
	static constexpr int VerifyTextWidth = STRLEN(VERIFY_LABEL) * CharWidth;
	static constexpr int VerifyTextLeft = (ScreenWidth - VerifyTextWidth) / 2;
	static constexpr int VerifyTextBottom = VerifyTextTop - CharHeight;

	struct Target
	{
		// For initializing
		#define TARGET(x, y) {{x, 0, 1, ScreenHeight}/*vert*/, {0, y, ScreenWidth, 1}/*horz*/}
		#define TARGET_MID(x, y) {{x, 0, 1, RestartTextTop}/*vert*/, {0, y, ScreenWidth, 1}/*horz*/}

		Area	vert;
		Area	horz;

		uint GetX()		{ return vert.Xpos; }
		uint GetY()		{ return horz.Ypos; }
	};

	#define TargX(n)	s_Targets[n].vert.Xpos
	#define TargY(n)	s_Targets[n].horz.Ypos

	struct TouchPoint
	{
		int		x;
		int		y;
	};

	// Imitate HotpostList
	struct CalibrateHotspotList
	{
		ushort	count;
		Hotspot	list[2];
	};

	enum CalibrateHotspots
	{
		HOTSPOT_Repeat,
		HOTSPOT_Done,
	};

	//*********************************************************************
	// Public interface
	//*********************************************************************
public:
	TouchCalibrate_t() : TouchCanvas(0, ScreenWidth, ScreenHeight, ScreenWidth, Color16bpp, (HotspotList *)&s_hotSpots) {}

public:
	void Open(bool fShowButtons = false)
	{
		int		flags;
		HotspotData	*pSpot;

		AllocIfNeeded(ScreenHeight);
		m_oldImage = GetMainImage();
		FillRect(this, GetViewArea(), BackColor);
		SetMainImage(this);
		DisablePip1();
		DisablePip2();
		
		// Add restart label
		SetupText(this, CCR0_CharHeight32 | CCR0_CharSet8859_1, 
			CCR1_CharHeightX1 | CCR1_CharWidthX1 | CCR1_CharBackgroundTransparent);
		SetForeColor(TextColor);
		SetTextPosition(RestartTextLeft, RestartTextTop);
		WriteString(RESTART_LABEL);

		if (fShowButtons)
			goto ShowButtons;

		for (;;) 
		{
			DrawDoneButton(SKIP_BTN_LABEL, SkipTextLeft);
			StartCalibration();

ShowButtons:
			DrawDoneButton(DONE_BTN_LABEL, DoneTextLeft);
			DrawCalButton();
			for (;;) 
			{
				// This loop display a target where screen is being touched
				flags = GetTouch(true);
				if (flags == AbortFlag)
					goto Abort;

				if (flags & TOUCH_Start)
				{
					pSpot = TestHit(pTouch->GetX(), pTouch->GetY());
					if (pSpot != NULL && pSpot != NOT_ON_CANVAS)
					{
						if (pSpot->id == HOTSPOT_Done)
							goto Abort;
						else
						{
							EraseCalButton();
							break;	// repeat
						}
					}
				}
			}
		} // for(;;) repeat calibration

Abort:
		DisableGraphicsCursor();				
		Eeprom.StartSave();
		SetMainImage(m_oldImage);
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void DrawCalButton()
	{
		FillRect(this, &s_areaRepeatBtn, ButtonColor);

		// Write the button label
		SetForeColor(ButtonTextColor);
		WriteReg(CCR1, CCR1_CharHeightX2 | CCR1_CharWidthX2 | CCR1_CharBackgroundTransparent);
		SetTextPosition(RepeatBtnLeft + RepeatTextLeft, RepeatBtnTop + ReapeatTextTop);
		WriteString(REPEAT_BTN_LABEL);
		
		// Now the verify message
		SetForeColor(TextColor);
		WriteReg(CCR1, CCR1_CharHeightX1 | CCR1_CharWidthX1 | CCR1_CharBackgroundTransparent);
		SetTextPosition(VerifyTextLeft, VerifyTextTop);
		WriteString(VERIFY_LABEL);
	}

	void DrawDoneButton(const char *psz, int left)
	{
		FillRect(this, &s_areaDoneBtn, ButtonColor);

		// Write the labels
		SetForeColor(ButtonTextColor);
		WriteReg(CCR1, CCR1_CharHeightX2 | CCR1_CharWidthX2 | CCR1_CharBackgroundTransparent);
		SetTextPosition(DoneBtnLeft + left, DoneBtnTop + DoneTextTop);
		WriteString(psz);
	}
	
	void EraseCalButton()
	{
		FillRect(this, &s_areaRepeat, BackColor);
	}

	void DrawTarget(const Target &target, bool fErase = false)
	{
		ulong color = fErase ? ScreenBackColor : TargetColor;
		FillRect(this, &target.horz, color);
		FillRect(this, &target.vert, color);
	}

	bool CalibrateTarget(const Target &target, TouchPoint &point)
	{
		Timer	tmr;
		int		flags;

		DrawTarget(target);

Restart:
		do 
		{
			flags = GetTouch();
			if (flags == AbortFlag)
			{
				DrawTarget(target, true);	// Erase it
				return true;
			}
		} while (!(flags & TOUCH_Start));

		// Screen touched, make 'em hold it
		tmr.Start();
		do 
		{
			flags = GetTouch();
			if (flags == AbortFlag)
				return true;
			if (!(flags & TOUCH_Touched))
				goto Restart;
		} while (!tmr.CheckDelay_ms(TouchTimeMs));

		// Held on target for required time, use final value
		point.x = pTouch->GetRawX();
		point.y = pTouch->GetRawY();

		// Now erase the target
		DrawTarget(target, true);
		return false;
	}

	static long DivLongLong(long long num, long den) NO_INLINE_ATTR
	{
		return ShiftIntRnd(num * 2 / den, 1);
	}

	static long DivScale(long num, long den)
	{
		return DivLongLong((long long)num << TouchShift, den);
	}

	bool StartCalibration()
	{
		int		val, k;
		long long	c1, c2, c3, baseX, baseY;

		if (CalibrateTarget(s_Targets[0], m_points[0]))
			return true;
		if (CalibrateTarget(s_Targets[1], m_points[1]))
			return true;
		if (CalibrateTarget(s_Targets[2], m_points[2]))
			return true;

		// Equations for 3-point touch calibration were found at
		// https://www.embedded.com/how-to-calibrate-touch-screens/
		//
		// A .pdf from Texas Instruments called "Calibration in touch-screen systems"
		// also has these equations with an explanation of how to extend it
		// to more points, with an example of 5 points.

		k = (m_points[0].x - m_points[2].x) * (m_points[1].y - m_points[2].y) -
			(m_points[1].x - m_points[2].x) * (m_points[0].y - m_points[2].y);

		val = (TargX(0) - TargX(2)) * (m_points[1].y - m_points[2].y) -
			(TargX(1) - TargX(2)) * (m_points[0].y - m_points[2].y);
		Eeprom.Data.TouchInit.scaleX.aScale = DivScale(val, k);

		val = (TargX(1) - TargX(2)) * (m_points[0].x - m_points[2].x) -
			(TargX(0) - TargX(2)) * (m_points[1].x - m_points[2].x);
		Eeprom.Data.TouchInit.scaleX.bScale = DivScale(val, k);

		val = (TargY(1) - TargY(2)) * (m_points[0].x - m_points[2].x) -
			(TargY(0) - TargY(2)) * (m_points[1].x - m_points[2].x);
		Eeprom.Data.TouchInit.scaleY.aScale = DivScale(val, k);

		val = (TargY(0) - TargY(2)) * (m_points[1].y - m_points[2].y) -
			(TargY(1) - TargY(2)) * (m_points[0].y - m_points[2].y);
		Eeprom.Data.TouchInit.scaleY.bScale = DivScale(val, k);

		// Final calculation exceeds int, use long long
		c1 = m_points[1].x * m_points[2].y - m_points[2].x * m_points[1].y;
		c2 = m_points[2].x * m_points[0].y - m_points[0].x * m_points[2].y;
		c3 = m_points[0].x * m_points[1].y - m_points[1].x * m_points[0].y;
		baseX = c1 * TargX(0) + c2 * TargX(1) + c3 * TargX(2);
		baseY = c1 * TargY(0) + c2 * TargY(1) + c3 * TargY(2);

		// compute rounded 32-bit result of division
		val = DivLongLong(baseX, k);
		Eeprom.Data.TouchInit.scaleX.base = val;

		val = DivLongLong(baseY, k);
		Eeprom.Data.TouchInit.scaleY.base = val;

		return false;
	}

	int GetTouch(bool fShow = false)
	{
		uint	flags;
		HotspotData	*pSpot;

		while (!(pTouch->Process()))
		{
			wdt_reset();
			if (Console.IsByteReady())
			{
				Console.DiscardReadBuf();
				return AbortFlag;
			}
		}
		
		flags = pTouch->GetTouch();
		if (flags & TOUCH_Start)
		{
			pSpot = TestHit(pTouch->GetX(), pTouch->GetY());
			if (pSpot != NULL && pSpot != NOT_ON_CANVAS && pSpot->id == HOTSPOT_Done)
				return AbortFlag;
		}

		// show cursor at touch position
		if (fShow && flags & TOUCH_Touched)
		{
			EnableGraphicsCursor(GTCCR_GraphicCursorSelect2);
			SetGraphicsCursorPosition(pTouch->GetX() - 16, pTouch->GetY() - 16);
		}
		else
			DisableGraphicsCursor();				

		return flags;
	}

	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	TouchCanvas *m_oldImage;
	TouchPoint	m_points[3];
	
	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const ScaleMatrix s_calibMatrix = { TouchScale, 0, 0 };

	inline static const Target s_Targets[3] =
	{
		TARGET(TouchEdgeOffsetX, BottomTarget),
		TARGET_MID(MiddleTargetX, TouchEdgeOffsetY),
		TARGET(RightTarget, MiddleTargetY),
	};

	inline static const Area s_areaDoneBtn = { DoneBtnLeft, DoneBtnTop, DoneBtnWidth, DoneBtnHeight };
	inline static const Area s_areaRepeatBtn = { RepeatBtnLeft, RepeatBtnTop, RepeatBtnWidth, RepeatBtnHeight };
	inline static const Area s_areaRepeat = { VerifyTextLeft, RepeatBtnTop, VerifyTextWidth, RepeatBtnHeight + TextVertMargin + CharHeight };

	inline static const CalibrateHotspotList s_hotSpots = { 2, {
		// Array of Hotspots
		{RepeatBtnLeft, RepeatBtnTop, RepeatBtnLeft + RepeatBtnWidth - 1, RepeatBtnTop + RepeatBtnHeight - 1,
			{HOTSPOT_Repeat, 0}},
		{DoneBtnLeft, DoneBtnTop, DoneBtnLeft + DoneBtnWidth - 1, DoneBtnTop + DoneBtnHeight - 1,
			{HOTSPOT_Done, 0}}
	}};
};


//****************************************************************************
// Create a single static instance of the above class
//****************************************************************************

class TouchCalibrate
{
public:
	static void Open(bool fShowButtons = false)		{ s_calibrate.Open(fShowButtons); }
protected:
	inline static TouchCalibrate_t	s_calibrate;
};
