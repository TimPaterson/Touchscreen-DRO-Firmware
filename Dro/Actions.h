//****************************************************************************
// Actions.h
//
// Created 12/20/2020 12:51:43 PM by Tim
//
//****************************************************************************

#pragma once

#include "LcdDef.h"
#include "ScreenMgr.h"
#include "PowerDown.h"
#include "ToolLib.h"
#include "TouchCalibrate.h"
#include "UpdateMgr.h"
#include "LoadGraphics.h"


class Actions
{
	static constexpr int InBufSize = 11;
	static constexpr int MemoryCount = 4;

	enum ActionState
	{
		AS_Empty,		// display clear, ready to accept entry
		AS_Entering,	// numeric entry in process, no operation
		AS_Value,		// display has completed value
	};

	enum Operators
	{
		OP_none = Key_equal,
		OP_plus = Key_plus,
		OP_minus = Key_minus,
		OP_mult = Key_mult,
		OP_divide = Key_divide,
	};

	// Each axis has an array of settings. This indexes into the array.
	enum SettingsAreas
	{
		AREA_Resolution,
		AREA_Invert,
		AREA_Correction,
		AREA_Count,
	};

	//*********************************************************************
	// Local types
	//*********************************************************************
protected:
	class CalcMemory : public NumberLine
	{
	public:
		CalcMemory(const Area &area, ulong backColor) :
			NumberLine(MainScreen, area, FONT_CalcSmall, ScreenForeColor, backColor)
		{}

	public:
		double GetVal()	{ return m_val; }

		void SetVal(double val)
		{
			m_val = val;

			ClearArea();
			if (val == 0)
				return;
			FormatValue(val);
			WriteString(s_arFormatBuf);
		}

	protected:
		double	m_val;
	};

	//*********************************************************************
	// Public interface
	//*********************************************************************
public:
	static bool HasCalcValue()  {return s_state != AS_Empty; }

public:
	static void Init()
	{
		ApplySettings();
		ShowAbsInc();
		ShowInchMetric();
		ShowDiameterRadius();
		Tools.Init();
		Files.Init();
	}

	static void TakeAction(int x, int y, uint flags)
	{
		PipInfo		*pPipInfo;

		if (flags & TOUCH_Start)
			TakeAction(x, y);
		else if (s_pCapture != NULL)
		{
			if (flags & TOUCH_End)
			{
				s_pCapture->EndCapture();
				s_pCapture = NULL;
			}
			else
			{
				pPipInfo = Lcd.GetPip(s_pCapture);
				s_pCapture->NewPosition(x - pPipInfo->x, y - pPipInfo->y);
			}
		}
	}

	static double GetCalcValue()
	{
		return fabs(ToValueStateClear());	// negative values not allowed
	}

	static void SetCalcValue(double val)
	{
		if (val != 0)
			ToValueState(val);
	}

	//*********************************************************************
	// Key function dispatcher
	//*********************************************************************
protected:
	static void TakeAction(int x, int y)
	{
		HotspotData	*pSpot;
		uint		group;
		uint		spot;
		double		val;
		AxisPos		*pAxis;
		AxisDisplay	*pDisplay;
		char		*pStr;
		bool		*pToggle;
		ushort		*pUs;

		pSpot = Lcd.ScreenTestHit(x, y);
		if (pSpot == NULL)
			return;

		group = pSpot->group;
		spot = pSpot->id;

		switch (group)
		{

		//*****************************************************************
		// Dispatch to other handlers
		//

		case HOTSPOT_GROUP_ToolDisplay:
			s_pCapture = Tools.ListCapture(x, y, (ScrollAreas)spot);
			return;

		case HOTSPOT_GROUP_FileDisplay:
			s_pCapture = Files.ListCapture(x, y, (ScrollAreas)spot);
			return;

		case HOTSPOT_GROUP_ToolLib:
			Tools.ToolAction(spot, x, y);
			return;

		case HOTSPOT_GROUP_Update:
			UpdateMgr::UpdateAction(spot, x, y);
			return;

		case HOTSPOT_GROUP_LoadGraphics:
			LoadGraphics::LoadGraphicsAction(spot, x, y);
			return;

		case HOTSPOT_GROUP_TimeSet:
			Tools.SetTime(spot);
			return;

		case HOTSPOT_GROUP_Keyboard:
			KeyboardMgr::KeyHit(spot);
			return;

		//*****************************************************************
		// Press a numeric entry key
		//

		case HOTSPOT_GROUP_Digit:
			if (s_state == AS_Value)
			{
				if (spot == Key_sign)
				{
					// Treat as operator, change sign
					ToValueState(-s_arg1);
					break;
				}
				// Otherwise start new entry
				ClearEntry();
			}

			s_state = AS_Entering;
			if (spot == Key_sign)
			{
				if (s_arEntryBuf[0] == '-')
				{
					s_arEntryBuf[0] = ' ';
					if (s_indBuf == 1)
						s_state =  AS_Empty;
				}
				else
					s_arEntryBuf[0] = '-';
			}
			else
			{
				if (s_indBuf >= InBufSize - 1)
					return;

				if (spot == Key_decimal)
				{
					if (s_fHaveDp)
						return;
					s_fHaveDp = true;
				}

				s_arEntryBuf[s_indBuf++] = spot;
				s_arEntryBuf[s_indBuf] = '\0';
			}
			s_CalcDisplay.ResetPosition();
			s_CalcDisplay.WriteString(s_arEntryBuf);
			break;

		//*****************************************************************
		// Press an operator key
		//

		case HOTSPOT_GROUP_Operator:
			if (s_state == AS_Empty)
				break;

			val = ToValueState();
			if (s_op != OP_none)
			{
				// Perform previous operation
				switch (s_op)
				{
				case OP_plus:
					val += s_arg1;
					break;

				case OP_minus:
					val = s_arg1 - val;
					break;

				case OP_mult:
					val *= s_arg1;
					break;

				default:	// OP_divide
					val = s_arg1 / val;
					break;
				}
				s_arg1 = val;
			}
			s_op = spot;
			if (spot == OP_none)
				ToValueState(val);
			else
				ClearEntry();
			break;

		//*****************************************************************
		// Press a special key
		//

		case HOTSPOT_GROUP_Edit:
			switch (spot)
			{
				case Key_pi:
					ToValueState(M_PI);
					break;

				case Key_backSpace:
					switch (s_state)
					{
					case AS_Entering:
						if (s_indBuf > 1)
						{
							s_indBuf--;
							if (s_fHaveDp && s_arEntryBuf[s_indBuf] == '.')
								s_fHaveDp = false;
							s_arEntryBuf[s_indBuf] = '\0';
							if (s_indBuf == 1 && s_arEntryBuf[0] == ' ')
								s_state = AS_Empty;
						}
						else if (s_arEntryBuf[0] == '-')
						{
							s_arEntryBuf[0] = ' ';
							s_state = AS_Empty;
						}
						break;

					default:
						if (s_op != OP_none)
						{
							s_op = OP_none;
							ToValueState(s_arg1);
						}
						break;
					}
					s_CalcDisplay.ClearArea();
					s_CalcDisplay.WriteString(s_arEntryBuf);
					break;

				default:	// Key_clear
					if (s_state == AS_Empty)
					{
						if (s_op != OP_none)
						{
							s_op = OP_none;
							ToValueState(s_arg1);
						}
					}
					else
						ClearEntry();
					break;
			}
			break;

		//*****************************************************************
		// Press one of the axis displays.
		//
		// If the calculator display is empty, this copies the axis value
		// to the calculator. Otherwise, it copies the calculator value
		// to the axis.

		case HOTSPOT_GROUP_Axis:
			pDisplay = AxisDisplay::Axes[spot];
			if (!pDisplay->IsVisible())
				return;

			if (s_state == AS_Empty)
				ToValueState(pDisplay->GetPosition());
			else
				pDisplay->SetPosition(ToValueStateClear());
			break;

		//*****************************************************************
		// Press one of the axis zero buttons.
		//

		case HOTSPOT_GROUP_AxisButton:
			AxisDisplay::Axes[spot]->SetPosition(0);
			break;

		//*****************************************************************
		// Press Undo list for an axis.
		//

		case HOTSPOT_GROUP_Undo:
			AxisDisplay::Axes[spot]->Undo();
			break;

		//*****************************************************************
		// Press a memory display
		//
		// If the calculator display is empty, this copies the memory value
		// to the calculator. Otherwise, it copies the calculator value
		// to the memory.

		case HOTSPOT_GROUP_Memory:
			spot -= Mem1;

			if (s_state == AS_Empty)
			{
				val = s_memories[spot].GetVal();
				if (val != 0)
					ToValueState(val);
			}
			else
				s_memories[spot].SetVal(ToValueState());
			break;

		//*****************************************************************
		// Press a tool cutting side key
		//

		case HOTSPOT_GROUP_ToolSide:
			Tools.SetToolSide(spot);
			return;

		//*****************************************************************
		// Settings screen
		//

		case HOTSPOT_GROUP_Disable:
			pAxis = AxisDisplay::AxisPositions[spot];
			pAxis->SetDisable(pAxis->IsDisabled() ^ true);
			ShowSettingsInfo();
			return;

		case HOTSPOT_GROUP_Resolution:
			pAxis = AxisDisplay::AxisPositions[spot];
			if (s_state == AS_Empty)
			{
				// Just reading the value
				val = pAxis->GetResolution();
				ToValueState(val);
				break;
			}
			else
			{
				// Setting the value
				val = ToValueStateClear();
				pAxis->SetResolution(val);
				ShowSettingsInfo();
				return;
			}

		case HOTSPOT_GROUP_Correction:
			pAxis = AxisDisplay::AxisPositions[spot];
			if (s_state == AS_Empty)
			{
				// Just reading the value
				val = pAxis->GetCorrectionPpm();
				ToValueState(val);
				break;
			}
			else
			{
				// Setting the value
				val = ToValueStateClear();
				if (!pAxis->SetCorrectionPpm(val))
					;	// UNDONE: Display error
				ShowSettingsInfo();
				return;
			}

		case HOTSPOT_GROUP_Invert:
			pAxis = AxisDisplay::AxisPositions[spot];
			pAxis->SetDirection(pAxis->GetDirection() ^ true);
			ShowSettingsInfo();
			return;

		case HOTSPOT_GROUP_Assign:
			s_assignAxis = spot;
			Lcd.EnablePip1(&LatheAssignList, LatheListLeft, LatheListTop + spot * LatheListHeight, true);
			return;

		case HOTSPOT_GROUP_AssignItem:
			Eeprom.Data.LatheAssign[s_assignAxis] = spot;
			// If this axis has been assigned to another sensor,
			// unassign that one.
			for (int i = 0; i < AxisPosCount; i++)
			{
				if (i != s_assignAxis && Eeprom.Data.LatheAssign[i] == spot)
					Eeprom.Data.LatheAssign[i] = LATHE_None;
			}
			Lcd.DisablePip1();
			ShowSettingsInfo();
			return;

		case HOTSPOT_GROUP_Machine:
			Eeprom.Data.fIsLathe = spot;
			Lcd.EnablePip2(spot == Mill ? &SettingsScreen : &LatheSettingsScreen, 0, 0);
			return;

		case HOTSPOT_GROUP_SettingToggle:
			switch (spot)
			{
			case HighlightXY:
				pToggle = &Eeprom.Data.fHighlightOffset;
				break;

			case CncCoordinates:
				pToggle = &Eeprom.Data.fCncCoordinates;
				break;

			case LatheShowT:
				pToggle = &Eeprom.Data.fLatheShowT;
				break;

			default:	// OffsetZ
				pToggle = &Eeprom.Data.fToolLenAffectsZ;
				break;
			}
			*pToggle ^= true;
			ShowSettingsInfo();
			Tools.ShowToolInfo();
			return;

		//*****************************************************************
		// Press a button not related to the calculator
		//

		default:
			switch (spot)
			{
			case ToolMenu:
				Tools.ShowToolLib();
				break;

			case InchMetric:
				Eeprom.Data.fIsMetric ^= true;
				ShowInchMetric();
				Tools.ChangeUnits();
				AxisDisplay::UpdateUndo();
				UpdateEeprom();
				break;

			case AbsInc:
				Eeprom.Data.OriginNum ^= 1;
				ShowAbsInc();
				AxisDisplay::UpdateUndo();
				UpdateEeprom();
				break;

			case DiameterRadius:
				Eeprom.Data.fLatheRadius ^= true;
				ShowDiameterRadius();
				UpdateEeprom();
				break;

			case CompoundEnable:
				Eeprom.Data.fCompoundFactor ^= true;
				ShowCompoundEnable();
				SetCompoundAngle(Eeprom.Data.CompoundAngle);
				UpdateEeprom();
				break;

			case CompoundAngle:
				if (!Eeprom.Data.fCompoundFactor)
					return;
				if (s_state == AS_Empty)
				{
					// Just reading the value
					ToValueState(Eeprom.Data.CompoundAngle);
				}
				else
				{
					// Setting the value
					val = ToValueStateClear();
					if (val <= 270 && val >= -90)
					{
						Eeprom.Data.CompoundAngle = val;
						SetCompoundAngle(val);
						UpdateEeprom();
					}
				}
				break;


			case Standby:
				PowerDown::EnterStandby();
				break;

			case Settings:
				if (s_isSettingsShown)
				{
					ApplySettings();
					s_isSettingsShown = false;
					Eeprom.StartSave();	// save all changes
				}
				else
				{
					// open settings
					s_isSettingsShown = true;
					Lcd.EnablePip2(Eeprom.Data.fIsLathe ? &LatheSettingsScreen : &SettingsScreen, 0, 0);
					ShowSettingsInfo();
				}
				break;

			case BrightUp:
				ChangeScreenBrightness(10);
				UpdateEeprom();
				break;

			case BrightDown:
				ChangeScreenBrightness(-10);
				UpdateEeprom();
				break;

			case MaxRpm:
				pUs = &Eeprom.Data.MaxRpm;
				goto SetRpm;
				
			case LatheMaxRpm:
				pUs = &Eeprom.Data.LatheMaxRpm;
SetRpm:
				if (s_state == AS_Empty)
				{
					// Just reading the value
					ToValueState(*pUs);
				}
				else
				{
					// Setting the value
					val = ToValueStateClear();
					if (val <= 60000 && val >= 100)
					{
						*pUs = lround(val);
						ShowSettingsInfo();
						Tools.ShowToolInfo();
					}
				}
				break;

			case TouchCal:
				// touch calibrations save changes to EEPROM
				// If mouse in use, go directly to calibration
				TouchCalibrate::Open(!Mouse.IsLoaded());
				s_isSettingsShown = false;
				break;

			case FirmwareUpdate:
				Eeprom.StartSave();	// save all changes
				UpdateMgr::Open();
				s_isSettingsShown = false;
				break;

			case HaveMouse:
				Lcd.DisablePip1();
				break;
			}
			return;
		}

		//*****************************************************************
		// Display full expression

		s_CalcText.ClearArea();
		pStr = s_arEntryBuf;
		if (s_arEntryBuf[0] == ' ')
			pStr++;

		if (s_op == OP_none)
			s_CalcText.WriteString(pStr);
		else
		{
			FormatValue(s_arg1);
			s_CalcText.printf("%s %c %s", s_arFormatBuf, s_op, pStr);
		}
	}

	//*********************************************************************
	// Helper functions - Main screen
	//*********************************************************************
protected:
	static void SelectImage(const Area *pAreaDst, const ColorImage *pSrc, uint index)
	{
		Lcd.SelectImage(&MainScreen, pAreaDst, pSrc, index);
	}

	static void UpdateEeprom()
	{
		Eeprom.StartSave();
	}

	static void ToValueState(double val)
	{
		int		cch;

		cch = FormatValue(val);
		if (s_arFormatBuf[0] != '-')
		{
			s_arEntryBuf[0] = ' ';
			memcpy(&s_arEntryBuf[1], s_arFormatBuf, cch + 1);
		}
		else
			memcpy(&s_arEntryBuf[0], s_arFormatBuf, cch + 1);

		s_CalcDisplay.ClearArea();
		s_CalcDisplay.WriteString(s_arEntryBuf);
		if (s_op == OP_none)
			s_arg1 = val;
		s_state = AS_Value;
	}

	static double ToValueState()
	{
		double val;

		if (s_state == AS_Value && s_op == OP_none)
			return s_arg1;

		val = atof(s_arEntryBuf);
		if (s_op == OP_none)
			s_arg1 = val;
		s_state = AS_Value;
		return val;
	}

	static double ToValueStateClear() NO_INLINE_ATTR
	{
		double val = ToValueState();
		ClearEntry();
		s_CalcText.ClearArea();
		return val;
	}

	static int FormatValue(double val) NO_INLINE_ATTR
	{
		int		cch;
		const char *pFmt;
		double	absVal;

		absVal = fabs(val);
		if (absVal > 99999999.0 || absVal < 0.00001)
		{
			if (absVal == 0.0)
			{
				s_arFormatBuf[0] = '0';
				cch = 1;
				goto SetNull;
			}
			pFmt = "%.2E";	// 3 significant digits (plus 5 char for exponent)
		}
		else if (absVal >= 1)
			pFmt = "%.8G";
		else
		{
			cch = snprintf(s_arFormatBuf, sizeof s_arFormatBuf, "%.7F", val);
			// strip trailing zeros
			while (s_arFormatBuf[cch - 1] == '0')
				cch--;
SetNull:
			s_arFormatBuf[cch] = '\0';
			return cch;
		}

		return snprintf(s_arFormatBuf, sizeof s_arFormatBuf, pFmt, val);
	}

	static void ClearEntry()
	{
		s_CalcDisplay.ClearArea();
		s_state = AS_Empty;
		s_fHaveDp = false;
		s_arEntryBuf[0] = ' ';
		s_arEntryBuf[1] = '\0';
		s_indBuf = 1;
	}

	static void ShowInchMetric()
	{
		SelectImage(&MainScreen_Areas.InchMetric, &InchMetricBtn, Eeprom.Data.fIsMetric);
		SelectImage(&MainScreen_Areas.SpeedDisplay, &SpeedDisplay, Eeprom.Data.fIsMetric);
		Lcd.SelectImage(&LatheMain, &LatheMain_Areas.SpeedUnits, &SpeedUnits, Eeprom.Data.fIsMetric);
	}

	static void ShowAbsInc()
	{
		SelectImage(&MainScreen_Areas.AbsInc, &Coord, Eeprom.Data.OriginNum);
	}

	static void ShowDiameterRadius()
	{
		Lcd.SelectImage(&LatheMain, &LatheMain_Areas.DiameterRadius, &DiameterRadiusBtn, Eeprom.Data.fLatheRadius);
	}

	static void ShowCompoundEnable()
	{
		if (AxisDisplay::HasCompound())
		{
			Lcd.SelectImage(&LatheMain, &LatheMain_Areas.CompoundEnable, &OnOffBtn, Eeprom.Data.fCompoundFactor);

			// See if we're sharing third display between Z' and T
			AxisDisplay::ShareT();
		}
		else
			Eeprom.Data.fCompoundFactor = false;
	}

	static void ShowMillLathe()
	{
		if (Eeprom.Data.fIsLathe)
			Lcd.EnablePip2(&LatheMain, 0, LatheMainTop);
		else
			Lcd.DisablePip2();
	}

	//*********************************************************************
	// Helper functions - Lathe screen
	//*********************************************************************
protected:
	static void SetCompoundAngle(double angle)
	{
		ToolLib::ShowCompoundAngle(angle);
		AxisDisplay::SetCompoundAngle(angle);
	}

	//*********************************************************************
	// Helper functions - Settings screen
	//*********************************************************************
protected:
	static void SettingsCheckBox(const Area &pAreaDst, bool f)
	{
		Lcd.SelectImage(&SettingsScreen, &pAreaDst, &CheckBox, f);
	}

	static void ShowAxisInfo(Canvas* pCanvas, const Area *pArea)
	{
		double	val;

		s_SettingDisplay.SetCanvas(pCanvas);
		for (int i = 0; i < AxisPosCount; i++, pArea += AREA_Count)
		{
			Lcd.SelectImage(pCanvas, &pArea[AREA_Invert], &CheckBox, g_arAxisInfo[i].Direction);

			val = AxisDisplay::AxisPositions[i]->GetResolution();
			s_SettingDisplay.PrintDbl("%.1f", val, pArea[AREA_Resolution]);

			val = (g_arAxisInfo[i].Correction - 1.0) * 1E6;
			s_SettingDisplay.PrintDbl("%+6.1f", val, pArea[AREA_Correction]);
		}
	}

	static void ShowLatheSettings()
	{
		const Area *pArea = &LatheSettingsScreen_Areas.Xassign;

		for (int i = 0; i < AxisPosCount; i++)
			Lcd.SelectImage(&LatheSettingsScreen, &pArea[i], &LatheAssignments, Eeprom.Data.LatheAssign[i]);

		ShowAxisInfo(&LatheSettingsScreen, &LatheSettingsScreen_Areas.Xresolution);
		
		Lcd.SelectImage(&LatheSettingsScreen, &LatheSettingsScreen_Areas.ShowT, &CheckBox, Eeprom.Data.fLatheShowT);

		s_SettingDisplay.PrintUint("%5i", Eeprom.Data.LatheMaxRpm, LatheSettingsScreen_Areas.LatheMaxRpm);
	}

	static void ShowMillSettings()
	{
		const Area *pArea = &SettingsScreen_Areas.Xdisable;

		for (int i = 0; i < AxisPosCount; i++)
			SettingsCheckBox(pArea[i], g_arAxisInfo[i].Disable);

		ShowAxisInfo(&SettingsScreen, &SettingsScreen_Areas.Xresolution);

		SettingsCheckBox(SettingsScreen_Areas.HighlightXY, Eeprom.Data.fHighlightOffset);
		SettingsCheckBox(SettingsScreen_Areas.OffsetZ, Eeprom.Data.fToolLenAffectsZ);
		SettingsCheckBox(SettingsScreen_Areas.CncCoordinates, Eeprom.Data.fCncCoordinates);

		s_SettingDisplay.PrintUint("%5i", Eeprom.Data.MaxRpm, SettingsScreen_Areas.MaxRpm);
	}

	static void ShowSettingsInfo()
	{
		ShowMillSettings();
		ShowLatheSettings();
	}

	static void ApplySettings()
	{
		AxisDisplay::AssignDisplays();
		ShowMillLathe();
		SetCompoundAngle(Eeprom.Data.CompoundAngle);
		ShowCompoundEnable();
	}

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static char		s_arEntryBuf[InBufSize] = " ";
	inline static char		s_arFormatBuf[InBufSize];
	inline static byte		s_indBuf = 1;
	inline static byte		s_op = OP_none;
	inline static byte		s_state;
	inline static bool		s_fHaveDp;
	inline static byte		s_assignAxis;	// lathe axis being assigned
	inline static bool		s_isSettingsShown;
	inline static double	s_arg1;
	inline static ListScroll	*s_pCapture;

	inline static CalcMemory s_memories[MemoryCount]
	{
		{MainScreen_Areas.Mem1, MemColorOdd},
		{MainScreen_Areas.Mem2, MemColorEven},
		{MainScreen_Areas.Mem3, MemColorOdd},
		{MainScreen_Areas.Mem4, MemColorEven},
	};
	inline static NumberLine s_CalcDisplay{MainScreen, MainScreen_Areas.CalcDisplay,
		FONT_Calculator, ScreenForeColor, CalcBackColor};
	inline static TextField	s_CalcText{MainScreen, MainScreen_Areas.CalcText,
		FONT_CalcSmall, ScreenForeColor, CalcBackColor};
	inline static NumberLineBlankZ s_SettingDisplay{SettingsScreen, SettingsScreen_Areas.MaxRpm,
		FONT_SettingsFont, SettingForeColor, SettingBackColor};
};
