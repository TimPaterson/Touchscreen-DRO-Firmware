//****************************************************************************
// AxisDisplay.h
//
// Created 12/19/2020 1:57:03 PM by Tim
//
//****************************************************************************

#pragma once

#include "AxisPos.h"
#include "LcdDef.h"
#include "TextField.h"
#include "HotspotList.h"
#include "ToolLib.h"


class AxisDisplay;

extern AxisDisplay	Xdisplay;
extern AxisDisplay	Ydisplay;
extern AxisDisplay	Zdisplay;
extern AxisPos		Xpos;
extern AxisPos		Ypos;
extern AxisPos		Zpos;
extern AxisPos		Qpos;


class AxisDisplay
{
	static constexpr int AxisDisplayCount = 3;
	static constexpr int UndoDisplays = 3;	// per column
	
	// This is used as an index into a list of graphics labels
	enum AxisLabels
	{
		LABEL_X,
		LABEL_Y,
		LABEL_Z,
		LABEL_Zprime,
		LABEL_T,
	};
	
	//*********************************************************************
	// Public interface
	//*********************************************************************
public:
	// Constructor
	AxisDisplay(const Area &axisArea, const Area &undoArea, const Area &buttonArea, 
			const Area &undoLabel) : 
		m_pAxisArea{&axisArea}, m_pButtonArea{&buttonArea}, m_pUndoArea{&undoArea}, 
			m_pUndoLabelArea{&undoLabel}
		{}

public:
	bool IsVisible()				{ return m_pAxisPos != NULL; }
	void SetTextColor(ulong color)	{ m_textColor = color; }
		
public:
	void UpdateDisplay()
	{
		if (IsVisible())
		{
			s_Display.SetArea(*m_pAxisArea);
			s_Display.SetTextColor(m_textColor);
			s_Display.PrintSigned(GetPosition(), 9,  m_pAxisPos->GetDecimals());
			
			if (m_isLatheX)
			{
				double val;
				
				// update RPM for current X at given SFM (really meters/min)
				val = m_pAxisPos->GetPosition();	// radius, mm or inch
				if (val <= 0)
					val = 0;
				else
				{
					val = Eeprom.Data.Sfm / (val * (2 * M_PI / (Eeprom.Data.fIsMetric ? 1000 : 1000 / 25.4)));
					val = std::min(val, (double)Eeprom.Data.LatheMaxRpm);
					ToolLib::ShowLatheRpm((uint)val);
				}
			}
		}
	}

	double GetPosition()
	{
		double pos = IsVisible() ? m_pAxisPos->GetPosition() : 0;
		if (m_isLatheX && !Eeprom.Data.fLatheRadius)
			pos *= 2;
		return pos;
	}

	void SetPosition(double pos)
	{
		if (m_isLatheX && !Eeprom.Data.fLatheRadius)
			pos /= 2;
		if (IsVisible() && m_pAxisPos->SetPosition(pos) != 0)
			DisplayUndo();
	}

	void Undo()
	{
		if (IsVisible() && m_pAxisPos->Undo())
			DisplayUndo();
	}
	
public:
	static void UpdateAll()
	{
		Xdisplay.UpdateDisplay();
		Ydisplay.UpdateDisplay();
		Zdisplay.UpdateDisplay();
	}

	static void UpdateUndo()
	{
		Xdisplay.DisplayUndo();
		Ydisplay.DisplayUndo();
		Zdisplay.DisplayUndo();
	}
	
	static void AssignDisplays()
	{
		int			curDisplay = 0;
		AxisPos*	pSense;
		
		s_isTshared = false;
		if (Eeprom.Data.fIsLathe)
		{
			// For lathe, sensor can be assigned any axis. We will
			// display them in order: X, Z, Z', T (choosing top 3).
			s_latheXpos = FindAssignment(LATHE_X);
			s_latheZpos = FindAssignment(LATHE_Z);
			s_latheZprimePos = FindAssignment(LATHE_Zprime);
			s_latheTpos = FindAssignment(LATHE_T);
			
			// Cross slide
			if (s_latheXpos != NULL)
				Axes[curDisplay++]->AssignDisplay(s_latheXpos, LABEL_X, true);
			
			// Carriage
			if (s_latheZpos != NULL)
				Axes[curDisplay++]->AssignDisplay(s_latheZpos, LABEL_Z);
			
			// Compound
			if (s_latheZprimePos != NULL)
			{
				Axes[curDisplay++]->AssignDisplay(s_latheZprimePos, LABEL_Zprime);
				
				if (s_latheXpos != NULL)
					s_latheXpos->SetSensor(s_latheZprimePos);
						
				if (s_latheZpos != NULL)
					s_latheZpos->SetSensor(s_latheZprimePos);
			}
			
			// Tailstock
			if (s_latheTpos != NULL)
			{
				if (curDisplay == AxisDisplayCount)
				{
					// Not enough displays for everybody
					if (Eeprom.Data.fLatheShowT)
					{
						curDisplay--;	// take over display Z' display
						goto AssignT;
					}
					else
						s_isTshared = true;
				}
				else
				{
			AssignT:
					Axes[curDisplay++]->AssignDisplay(s_latheTpos, LABEL_T);
				}
			}
		}
		else
		{
			// For mill, sensor order is fixed: X, Y, Z, Q, although
			// some may be skipped.
			for (int i = 0; i < AxisPosCount; i++)
			{
				pSense = AxisPositions[i];
				if (!pSense->IsDisabled())
				{
					if (curDisplay < AxisDisplayCount)
					{
						pSense->SetSensor(NULL);
						Axes[curDisplay++]->AssignDisplay(pSense, i);
					}
					else
					{
						Zpos.SetSensor(pSense);
						Zpos.SetFactor(1.0);
						break;
					}
				}
			}
		}
		
		for (; curDisplay < AxisDisplayCount; curDisplay++)
			Axes[curDisplay]->AssignDisplay(NULL, 0);
			
		for (int i = 0; i < AxisPosCount; i++)
			AxisPositions[i]->SetRounding();
	}
	
	static void SetCompoundAngle(double angle)
	{
		double sine, cosine;
			
		if (Eeprom.Data.fCompoundFactor)
		{
			angle *= M_PI / 180;	// convert degrees to radians
			__builtin_sincos(angle, &sine, &cosine);
		}
		else
		{
			sine = 0;
			cosine = 0;
		}
			
		if (s_latheXpos != NULL)
			s_latheXpos->SetFactor(sine);
						
		if (s_latheZpos != NULL)
			s_latheZpos->SetFactor(cosine);
	}
	
	static void ShareT()
	{
		if (Eeprom.Data.fIsLathe)
		{
			if (Eeprom.Data.fCompoundFactor)
			{
				if (s_isTshared)
					Zdisplay.AssignDisplay(s_latheTpos, LABEL_T);
				else
					Zdisplay.AssignDisplay(NULL, 0);
			}
			else
				Zdisplay.AssignDisplay(s_latheZprimePos, LABEL_Zprime);
		}
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void DisplayUndo()
	{
		if (IsVisible())
		{
			for (uint i = 0; i < UndoDisplays; i++)
			{
				double pos = m_pAxisPos->GetUndoValue(i);
				if (m_isLatheX && !Eeprom.Data.fLatheRadius)
					pos *= 2;
				s_UndoDisplay.PrintSigned(pos, 8, m_pAxisPos->GetDecimals() - 1, m_pUndoArea[i]);
			}
		}
	}

	void AssignDisplay(AxisPos *pAxis, int labelIndex, bool isLatheX = false)
	{
		m_pAxisPos = pAxis;
		m_isLatheX = isLatheX;
		
		if (IsVisible())
		{
			Lcd.FillRect(&MainScreen, m_pAxisArea, AxisBackColor);
			Lcd.FillRect(&MainScreen, m_pUndoArea, UndoBackColor);
			Lcd.SelectImage(&MainScreen, m_pButtonArea, &AxisZero, labelIndex);
			Lcd.SelectImage(&MainScreen, m_pUndoLabelArea, &UndoLabels, labelIndex);
		}
		else
		{
			Lcd.FillRect(&MainScreen, m_pAxisArea, ScreenBackColor);
			Lcd.FillRect(&MainScreen, m_pButtonArea, ScreenBackColor);
			Lcd.FillRect(&MainScreen, m_pUndoArea, ScreenBackColor);
			Lcd.FillRect(&MainScreen, m_pUndoLabelArea, ScreenBackColor);
		}
	}
	
protected:
	static AxisPos *FindAssignment(int assign)
	{
		for (int i = 0; i < AxisPosCount; i++)
		{
			if (Eeprom.Data.LatheAssign[i] == assign)
				return AxisPositions[i];
		}
		return NULL;
	}
	
	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	bool		m_isLatheX;
	AxisPos		*m_pAxisPos;
	ulong		m_textColor;
	const Area	*m_pAxisArea;
	const Area	*m_pButtonArea;
	const Area	*m_pUndoArea;
	const Area	*m_pUndoLabelArea;

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static bool		s_isTshared;
	inline static AxisPos*	s_latheXpos;
	inline static AxisPos*	s_latheZpos;
	inline static AxisPos*	s_latheZprimePos;
	inline static AxisPos*	s_latheTpos;
	
	inline static NumberLine		s_Display{MainScreen, MainScreen_Areas.Xdisplay, 
		FONT_DigitDisplay, AxisForeColor, AxisBackColor};

	inline static NumberLineBlankZ	s_UndoDisplay{MainScreen, MainScreen_Areas.UndoX1, 
		FONT_CalcSmall, UndoTextColor, UndoBackColor};

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
public:		
	inline static AxisDisplay* const Axes[AxisDisplayCount] = { &Xdisplay, &Ydisplay, &Zdisplay };
		
	inline static AxisPos* const AxisPositions[AxisPosCount] = { &Xpos, &Ypos, &Zpos, &Qpos };
};
