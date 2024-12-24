//****************************************************************************
// TouchCalibrate.h
//
// Created 3/5/2021 3:00:10 PM by Tim
//
//****************************************************************************

#pragma once

#include "ScreenMgr.h"
#include "UsbDro.h"


class TouchCalibrate_t : public TouchCanvas, ScreenMgr
{
	// Calibration target positions
	static constexpr int LeftTargetX = ScreenWidth / 10;
	static constexpr int TopTargetY = ScreenHeight / 10;
	static constexpr int RightTargetX = ScreenWidth - LeftTargetX - 1;
	static constexpr int BottomTargetY = ScreenHeight - TopTargetY - 1;
	static constexpr int MiddleTargetX = ScreenWidth / 2;
	static constexpr int MiddleTargetY = ScreenHeight / 2;

	// List of 3 or more points for calibration. Excellent results have been
	// seen with 5 points. Each point adds about 40 bytes of constant data
	// in program space.
	#define TARGET_POINTS /* x, y, index */ \
		TARGET(LeftTargetX,		BottomTargetY,	0) \
		TARGET(LeftTargetX,		TopTargetY,		1) \
		TARGET(MiddleTargetX,	MiddleTargetY,	2) \
		TARGET(RightTargetX,	TopTargetY,		3) \
		TARGET(RightTargetX,	BottomTargetY,	4) 

	// This enum is for counting the number of calibration points
	enum
	{
		#define TARGET(x, y, index)	TargetPoint_##index,
		TARGET_POINTS
		#undef TARGET

		CalibrationPoints	// This will be set to the number of points
	};

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
	#define RESTART_LABEL		"Cycle power or click mouse to restart calibration"
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
	// Define the button to quit
	static constexpr int DoneBtnHeight = RepeatBtnHeight / 2;
	static constexpr int DoneBtnWidth = (STRLEN(DONE_BTN_LABEL) + 2) * ButtonCharWidth;
	static constexpr int DoneBtnLeft = (ScreenWidth - DoneBtnWidth) / 2;
	static constexpr int DoneBtnTop = ScreenHeight - DoneBtnHeight;
	// Positions within button
	static constexpr int ReapeatTextTop = (RepeatBtnHeight - ButtonCharHeight) / 2;
	static constexpr int RepeatTextLeft = (RepeatBtnWidth - STRLEN(REPEAT_BTN_LABEL) * ButtonCharWidth) / 2;
	static constexpr int DoneTextTop = (DoneBtnHeight - ButtonCharHeight) / 2;
	static constexpr int DoneTextLeft = (DoneBtnWidth - STRLEN(DONE_BTN_LABEL) * ButtonCharWidth) / 2;
	// Position on screen
	static constexpr int TextVertMargin = 6;
	static constexpr int RestartTextTop = DoneBtnTop - CharHeight - TextVertMargin;
	static constexpr int RestartTextLeft = (ScreenWidth - STRLEN(RESTART_LABEL) * CharWidth) / 2;
	static constexpr int VerifyTextTop = RepeatBtnTop + RepeatBtnHeight + TextVertMargin;
	static constexpr int VerifyTextWidth = STRLEN(VERIFY_LABEL) * CharWidth;
	static constexpr int VerifyTextLeft = (ScreenWidth - VerifyTextWidth) / 2;
	static constexpr int VerifyTextBottom = VerifyTextTop - CharHeight;

	// Used to draw the target on the screen
	struct Target
	{
		Area	vert;
		Area	horz;
	};

	// Imitate HotspotList
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

		AllocIfNeeded();
		SetupText(this, CCR0_CharHeight32 | CCR0_CharSet8859_1, 
			CCR1_CharHeightX1 | CCR1_CharWidthX1 | CCR1_CharBackgroundTransparent);
		DisablePip1();
		DisablePip2();
		s_oldImage = GetMainImage();
		SetMainImage(this);

		for (;;) 
		{
			if (!fShowButtons)
				StartCalibration();
			fShowButtons =  false;

			DrawButtons();
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

						// must be HOTSPOT_Repeat
						break;	// repeat calibration
					}
				}
			}
		} // for(;;) repeat calibration

Abort:
		if (Mouse.IsLoaded())
			Lcd.EnableGraphicsCursor(GTCCR_GraphicCursorSelect1);
		else
			DisableGraphicsCursor();
		Eeprom.StartSave();
		SetMainImage(s_oldImage);
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void BlankScreen()	
	{ 
		FillRect(this, GetViewArea(), BackColor); 
		// Add restart label
		SetForeColor(TextColor);
		WriteReg(CCR1, CCR1_CharHeightX1 | CCR1_CharWidthX1 | CCR1_CharBackgroundTransparent);
		SetTextPosition(RestartTextLeft, RestartTextTop);
		WriteString(RESTART_LABEL);
	}

	void DrawButtons()
	{
		BlankScreen();

		// Write the verify message. BlankScreen() sets X1 text.
		SetTextPosition(VerifyTextLeft, VerifyTextTop);
		WriteString(VERIFY_LABEL);

		// Draw buttons
		FillRect(this, &s_areaRepeatBtn, ButtonColor);
		FillRect(this, &s_areaDoneBtn, ButtonColor);

		// Write the labels
		SetForeColor(ButtonTextColor);
		WriteReg(CCR1, CCR1_CharHeightX2 | CCR1_CharWidthX2 | CCR1_CharBackgroundTransparent);
		SetTextPosition(RepeatBtnLeft + RepeatTextLeft, RepeatBtnTop + ReapeatTextTop);
		WriteString(REPEAT_BTN_LABEL);

		SetTextPosition(DoneBtnLeft + DoneTextLeft, DoneBtnTop + DoneTextTop);
		WriteString(DONE_BTN_LABEL);
	}

	void DrawTarget(const Target &target)
	{
		FillRect(this, &target.horz, TargetColor);
		FillRect(this, &target.vert, TargetColor);
	}

	bool CalibrateTarget(int index)
	{
		Timer	tmr;
		int		flags;

		BlankScreen();
		DrawTarget(s_Targets[index]);

Restart:
		do 
		{
			flags = GetTouch();
			if (flags == AbortFlag)
				return true;
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
		SaveCalPoint(pTouch->GetRawX(), pTouch->GetRawY(), index);
		
		return false;
	}

	bool StartCalibration()
	{
		for (int i = 0; i < CalibrationPoints; i++)
		{
			if (CalibrateTarget(i))
				return true;
		}

		// A .pdf from Texas Instruments called "Calibration in touch-screen systems"
		// has the matrix equations for calibration with 3 or more points.

		MatrixMultiply(Transposed, TouchPoints, Extended, CalibrationPoints, 3);
		MatrixInverse();
		MatrixMultiply(Inverse, Transposed, Solution, 3, CalibrationPoints);
		MatrixMultiply(Solution, Targets, TouchPoints, CalibrationPoints, 2);

		Eeprom.Data.TouchInit.scaleX.aScale = lround(TouchPoints[0][0] * TouchScale);
		Eeprom.Data.TouchInit.scaleX.bScale = lround(TouchPoints[1][0] * TouchScale);
		Eeprom.Data.TouchInit.scaleX.base =   lround(TouchPoints[2][0]);

		Eeprom.Data.TouchInit.scaleY.bScale = lround(TouchPoints[0][1] * TouchScale);
		Eeprom.Data.TouchInit.scaleY.aScale = lround(TouchPoints[1][1] * TouchScale);
		Eeprom.Data.TouchInit.scaleY.base =	  lround(TouchPoints[2][1]);

		return false;
	}

	int GetTouch(bool fShow = false)
	{
		uint	flags;

		while (!(pTouch->Process()))
		{
			wdt_reset();
			if (Console.IsByteReady())
			{
				Console.DiscardReadBuf();
				return AbortFlag;
			}
			
			if (UsbPort.Process() == HOSTACT_MouseChange)
			{
				if (Mouse.GetButtons().btnStart & BUTTON_Left)
					return AbortFlag;
			}
		}
		
		flags = pTouch->GetTouch();

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

protected:
	// A global buffer is used for intermediate results during calibration.
	// This struct defines the layout.
	//
	struct CalibrateData
	{
		double TouchPointMatrix[CalibrationPoints][3];
		double TransposedMatrix[3][CalibrationPoints];
		double ExtendedMatrix[3][6];	// Inverse is the right half of each row
		double SolutionMatix[3][CalibrationPoints];
	};

	// The matrices used for calibration have an array of pointers pointing to
	// each row. This allows MatrixInverse() to use an "extended" matrix where
	// each row is double length, with the first half the inputs and the second
	// half the outputs. So Extended is an array pointing to the first entry
	// of each row, and Inverse points to the fourth entry in each row.
	//
	// All row pointer arrays are in fixed and therefore in flash.
	//
	static void MatrixInverse()
	{
		// Extend this matrix with the identity matrix
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
				Inverse[i][j] = i == j ? 1 : 0;
		}

		// scale and subtract a row to zero out an element of another row
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				if (i != j)
				{
					double temp = Extended[j][i] / Extended[i][i];
					for (int k = 0; k < 6; k++)
						Extended[j][k] -= Extended[i][k] * temp;
				}
			}
		}

		// scale each row by its one non-zero element
		for (int i = 0; i < 3; i++)
		{
			double temp = Extended[i][i];
			for (int j = 3; j < 6; j++)
				Extended[i][j] /= temp;
		}
	}

	static void MatrixMultiply(const double * const in1[], const double * const in2[], double * const out[], int in1Cols, int in2Cols)
	{
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < in2Cols; j++)
			{
				double sum = 0;
				for (int k = 0; k < in1Cols; k++)
					sum += in1[i][k] * in2[k][j];
				out[i][j] = sum;
			}
		}
	}

	// map the data to the global buffer
	#define DATA	(*(CalibrateData *)&g_FileBuf)

	static void SaveCalPoint(ushort x, ushort y, int index)
	{
		// when saving the data, also save it in a transposed matrix
		DATA.TouchPointMatrix[index][0] = DATA.TransposedMatrix[0][index] = (double)x;
		DATA.TouchPointMatrix[index][1] = DATA.TransposedMatrix[1][index] = (double)y;
		DATA.TouchPointMatrix[index][2] = DATA.TransposedMatrix[2][index] = 1;
	}

	// convenient for debugging
	static void PrintMatrix(double * const in[], int rows, int cols)
	{
		for (int i = 0; i < rows; i++)
		{
			for (int j = 0; j < cols; j++)
				printf("%g ", in[i][j]);
			printf("\n");
		}
	}

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static TouchCanvas *s_oldImage;
	
	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	// array of target points as used by the drawing engine
	inline static const Target s_Targets[CalibrationPoints] =
	{
		#define TARGET(x, y, i) {{x, 0, 1, ScreenHeight}, {0, y, ScreenWidth, 1}},

		TARGET_POINTS

		#undef TARGET
	};

	// array of target points as used for matrix math
	inline static const double TargetMatrix[CalibrationPoints][2] =
	{
		#define TARGET(x, y, i)	{x, y},

		TARGET_POINTS

		#undef TARGET
	};

	// array of pointers to first element in each row of TargetMatrix
	inline static const double * const Targets[CalibrationPoints] =
	{
		#define TARGET(x, y, i)	TargetMatrix[i],

		TARGET_POINTS

		#undef TARGET
	};

	// array of pointers to first element in each row of TouchPointMatrix
	inline static double * TouchPoints[CalibrationPoints] =
	{
		#define TARGET(x, y, i)	DATA.TouchPointMatrix[i],

		TARGET_POINTS

		#undef TARGET
	};

	// array of pointers to first element in each row of  corresponding matrix

	inline static double * const Transposed[3] = 
		{ DATA.TransposedMatrix[0], DATA.TransposedMatrix[1], DATA.TransposedMatrix[2] };

	inline static double * const Extended[3] = 
		{ DATA.ExtendedMatrix[0], DATA.ExtendedMatrix[1], DATA.ExtendedMatrix[2] };

	inline static double * const Inverse[3] = 
		{ &DATA.ExtendedMatrix[0][3], &DATA.ExtendedMatrix[1][3], &DATA.ExtendedMatrix[2][3] };

	inline static double * const Solution[3] = 
		{ DATA.SolutionMatix[0], DATA.SolutionMatix[1], DATA.SolutionMatix[2] };

	#undef DATA

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
	static TouchCanvas *GetCanvas()					{ return &s_calibrate; }
	static void Open(bool fShowButtons = false)		{ s_calibrate.Open(fShowButtons); }
protected:
	inline static TouchCalibrate_t	s_calibrate;
};
