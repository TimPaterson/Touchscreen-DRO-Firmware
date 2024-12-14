//****************************************************************************
// SplashScreen.h
//
// Created 12/13/2024 6:40:53 PM by tp-ms
//
//****************************************************************************

#pragma once


class SplashScreen : ScreenMgr
{
	static constexpr int TitleLeft = 170;
	static constexpr int TitleTop = 220;
	static constexpr int TitleWidth = ScreenWidth - 2 * TitleLeft;
	static constexpr int TitleHeight = ScreenHeight - 2 * TitleTop;
	static constexpr int AuthorTop = 460;
	static constexpr int UrlLeft = 60;
	static constexpr int UrlTop = 560;
	static constexpr int TextColor = 0x00ff00;
	static constexpr int BackColor = 0xcf0000;

	#define TitleString		"Smart Digital Readout"
	#define AuthorString	"   by Tim Paterson"
	#define UrlString		"https://github.com/TimPaterson/TouchscreenDigitalReadout"

public:
	static void Display()
	{
		TouchCanvas	*pCanvas;

		// Put up a splash screen, using the touch calibration canvas
		pCanvas = TouchCalibrate::GetCanvas();
		pCanvas->AllocIfNeeded();
		s_text.SetCanvas(pCanvas);
		FillRect(pCanvas, pCanvas->GetViewArea(), BackColor);
		SetMainImage(pCanvas);
		DisplayOn();

		// Title
		s_text.printf(TitleString);

		// Author
		s_areaTitle.Ypos = AuthorTop;
		s_text.ResetPosition();
		s_text.printf(AuthorString);

		// URL
		s_text.SetCharSize(CCR0_CharHeight32 | CCR0_CharSet8859_1, CCR1_CharHeightX1 | CCR1_CharWidthX1);
		s_areaTitle.Ypos = UrlTop;
		s_areaTitle.Xpos = UrlLeft;
		s_text.ResetPosition();
		s_text.printf(UrlString);
	}

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static Area s_areaTitle  = { TitleLeft, TitleTop, TitleWidth , TitleHeight };

	inline static TextFieldFixed s_text {*(TouchCanvas	*)NULL, s_areaTitle, TextColor, BackColor,
		CCR0_CharHeight32 | CCR0_CharSet8859_1, CCR1_CharHeightX2 | CCR1_CharWidthX2};

};
