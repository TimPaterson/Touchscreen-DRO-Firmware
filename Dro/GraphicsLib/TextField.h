//****************************************************************************
// TextField.h
//
// Created 11/29/2020 10:01:40 AM by Tim
//
//****************************************************************************

#pragma once

#include "ScreenMgr.h"
#include "FontInfo.h"
#include "..\HotspotList.h"

//#define FONT_BPP_8

#ifdef FONT_BPP_8
#define FONT_DEPTH		Color8bpp
#define FONT_BIT_START	7
#else
#define FONT_DEPTH		Color16bpp
#define FONT_BIT_START	15
#endif


EXTERN_C FontInfo *FontList[];

//****************************************************************************

class TextField : public ScreenMgr
{
public:
	TextField(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, ulong backColor):
		TextField(canvas, area, font, foreColor, backColor,
		({union {void (TextField::*mf)(byte); _fdev_put_t *p;} u = {&TextField::WriteCharActive}; u.p;}))
		{}

protected:
	TextField(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, ulong backColor, _fdev_put_t *put):
		m_pCanvas{&canvas},
		m_pFontInfo{&font},
		m_pArea{&area},
		m_foreColor{foreColor},
		m_backColor{backColor},
		m_file{{{put}}, this, 0, _FDEV_SETUP_WRITE},
		m_curPosX{area.Xpos},
		m_curPosY{area.Ypos},
		m_spaceWidth{0},
		m_fTransparent{false}
		{}

	//*********************************************************************
	// Public interface
	//*********************************************************************3
public:
	void SetCanvas(Canvas* pCanvas)	{ m_pCanvas = pCanvas; }

public:
	void SetFont(FontInfo &font)
	{
		m_pFontInfo = &font;
		SetSpaceWidth();
	}

	void SetBackgroundTransparent(bool fTransparent)
	{
		m_fTransparent = fTransparent;
	}

	void SetSpaceWidth(uint width = 0)
	{
		m_spaceWidth = width;
	}

	uint GetSpaceWidth()
	{
		return m_spaceWidth;
	}

	void SetTextColor(ulong color)
	{
		m_foreColor = color;
	}

	void SetTextBackcolor(ulong color)
	{
		m_backColor = color;
	}

	void SetArea(const Area &area)
	{
		m_pArea = &area;
		ResetPosition();
	}

	void ResetPosition()
	{
		m_curPosX = m_pArea->Xpos;
		m_curPosY = m_pArea->Ypos;
	}

	void MoveXposition(int cntPx)
	{
		m_curPosX += cntPx;
	}

	void NewLine()
	{
		m_curPosY += m_pFontInfo->Height;
	}

	void WriteChar(byte ch)
	{
		MakeActive();
		WriteCharActive(ch);
	}

	void WriteString(const char *psz)
	{
		byte	ch;

		MakeActive();
		for (;;)
		{
			ch = *psz++;
			if (ch == 0)
				return;
			WriteCharActive(ch);
		}
	}

	void WriteBlankSpace(ushort width)
	{
		Area area = {m_curPosX, m_pArea->Ypos, width, m_pArea->Height};
		m_curPosX += width;
		FillRect(m_pCanvas, &area, m_backColor);
	}

	virtual int printf(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)))
	{
		va_list ap;
		int i;

		MakeActive();
		va_start(ap, fmt);
		i = vfprintf(&m_file, fmt, ap);
		va_end(ap);

		return i;
	}

	int GetCharWidth(byte ch)
	{
		ch -= m_pFontInfo->FirstChar;
		if (ch > m_pFontInfo->LastChar)
			return 0;

		if (ch + m_pFontInfo->FirstChar == ' ' && m_spaceWidth != 0)
			return m_spaceWidth;
		return m_pFontInfo->arWidths[ch];
	}

	int GetStdCharWidth(byte ch)
	{
		return m_pFontInfo->arWidths[ch - m_pFontInfo->FirstChar];
	}

	int GetStringWidth(const char *psz)
	{
		byte	ch;
		int		len;

		for (len = 0;;)
		{
			ch = *psz++;
			if (ch == 0)
				break;
			len += GetCharWidth(ch);
		}
		return len;
	}

	void ClearArea()
	{
		FillArea(m_backColor);
		ResetPosition();
	}

	void ClearToEnd()
	{
		ClearToEnd(m_curPosX);
	}

	void ClearToEnd(uint posX)
	{
		SetBteDest(m_pCanvas);
		SetForeColor(m_backColor);
		WriteReg(BTE_CTRL1, BTE_CTRL1_OpcodeSolidFill);
		WriteRegXY(DT_X0, posX, m_pArea->Ypos);
		WriteRegXY(BTE_WTH0, m_pArea->Xpos + m_pArea->Width - posX, m_pArea->Height);
		WriteReg(BTE_CTRL0, BTE_CTRL0_Enable);
		WaitWhileBusy();
	}

	void FillArea(ulong color)
	{
		FillRect(m_pCanvas, m_pArea, color);
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void MakeActive()
	{
		byte	ctrl;
		ulong	color;

		SetBteDest(m_pCanvas);
		WriteReg16(DT_Y0, m_curPosY);
		SetBteSrc0((Image *)m_pFontInfo, FONT_DEPTH);
		WriteReg16(S0_Y0, 0);
		WriteReg16(BTE_HIG0, m_pFontInfo->Height);
		SetForeColor(m_foreColor);
		if (m_fTransparent)
		{
			color = ~m_foreColor;	// make sure it's different
			ctrl = BTE_CTRL1_OpcodeMemoryCopyExpandMonoTransparent;
		}
		else
		{
			color = m_backColor;
			ctrl = BTE_CTRL1_OpcodeMemoryCopyExpandMono;
		}
		SetBackColor(color);
		WriteReg(BTE_CTRL1, (FONT_BIT_START << BTE_CTRL1_BitStartShift) | ctrl);
	}

	void WriteCharActive(byte ch)
	{
		byte	width;
		int		remain;

		if (ch == '\n')
		{
			ResetPosition();
			return;
		}

		ch -= m_pFontInfo->FirstChar;
		if (ch > m_pFontInfo->LastChar)
			return;

		WriteReg16(S0_X0, ch * m_pFontInfo->CharStride);
		if (ch + m_pFontInfo->FirstChar == ' ' && m_spaceWidth != 0)
			width = m_spaceWidth;
		else
			width = m_pFontInfo->arWidths[ch];
		remain = m_pArea->Xpos + m_pArea->Width - m_curPosX;
		if (remain <= 0)
			return;
		if (remain < width)
			width = remain;
		WriteReg16(BTE_WTH0, width);
		WriteReg16(DT_X0, m_curPosX);
		m_curPosX += width;
		WriteReg(BTE_CTRL0, BTE_CTRL0_Enable);
		WaitWhileBusy();
	}

	//*********************************************************************
	// instance data
	//*********************************************************************
protected:
	Canvas		*m_pCanvas;
	FontInfo	*m_pFontInfo;
	const Area	*m_pArea;
	ulong		m_foreColor;
	ulong		m_backColor;
	FILE		m_file;
	ushort		m_curPosX;
	ushort		m_curPosY;
	byte		m_spaceWidth;
	bool		m_fTransparent;
};


//****************************************************************************


class NumberLine : public TextField
{
public:
	NumberLine(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, ulong backColor):
		TextField(canvas, area, font, foreColor, backColor)
	{
		SetSpaceWidth(GetStdCharWidth('0'));
	}

public:
	void SetFont(FontInfo &font)
	{
		TextField::SetFont(font);
		SetSpaceWidth(GetStdCharWidth('0'));
	}

	void ShiftMinus()
	{
		MoveXposition(GetStdCharWidth('0') - GetStdCharWidth('-'));
	}

	int PrintSigned(double val, int width, int decimals)
	{
		if (val >= 0)
		{
			WriteBlankSpace(GetStdCharWidth('-'));
			width--;
		}

		return printf("%*.*f", width, decimals, val);
	}
	
	int PrintDbl(const char *fmt, double val)
	{
		if (val < 0)
			WriteBlankSpace(GetStdCharWidth('0') - GetStdCharWidth('-'));
			
		return printf(fmt, val);
	}

	void WriteString(const char *psz)
	{
		if (*psz == '-')
			ShiftMinus();

		TextField::WriteString(psz);
	}
};


//****************************************************************************

class NumberLineBlankZ : public NumberLine
{
public:
	NumberLineBlankZ(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, ulong backColor):
		NumberLine(canvas, area, font, foreColor, backColor)
	{
		//SetBackgroundTransparent(true);
	}

public:
	int PrintSigned(double val, int width, int decimals)
	{
		ClearArea();
		if (val == 0)
			return 0;

		return NumberLine::PrintSigned(val, width, decimals);
	}

	int PrintSigned(double val, int width, int decimals, const Area &area)
	{
		SetArea(area);
		return PrintSigned(val, width, decimals);
	}

	int PrintDbl(const char *fmt, double val)
	{
		ClearArea();
		if (val == 0)
			return 0;

		if (val < 0)
			ShiftMinus();
		return printf(fmt, val);
	}

	int PrintDbl(const char *fmt, double val, const Area &area)
	{
		SetArea(area);
		return PrintDbl(fmt, val);
	}

	int PrintInt(const char *fmt, int val)
	{
		if (val == 0)
		{
			ClearArea();
			return 0;
		}

		if (val < 0)
			ShiftMinus();
		return printf(fmt, val);
	}

	int PrintInt(const char *fmt, int val, const Area &area)
	{
		SetArea(area);
		return PrintInt(fmt, val);
	}

	int PrintUint(const char *fmt, uint val)
	{
		ClearArea();
		if (val == 0)
			return 0;

		return printf(fmt, val);
	}

	int PrintUint(const char *fmt, uint val, const Area &area)
	{
		SetArea(area);
		return PrintInt(fmt, val);
	}
};


//****************************************************************************

class TextFieldFixed : public TextField
{
public:
	TextFieldFixed(Canvas &canvas, const Area &area, ulong foreColor, ulong backColor, byte ccr0, byte ccr1, bool isMultiLine = false):
		TextFieldFixed(canvas, area, FONT_CalcSmall, foreColor, backColor, ccr0, ccr1, isMultiLine,
		({union {void (TextFieldFixed::*mf)(byte); _fdev_put_t *p;} u = {&TextFieldFixed::WriteCharFixed}; u.p;}))
		{}

	TextFieldFixed(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, ulong backColor):
		TextField(canvas, area, font, foreColor, backColor)
		{}

protected:
	TextFieldFixed(Canvas &canvas, const Area &area, FontInfo &font, ulong foreColor, 
		ulong backColor, byte ccr0, byte ccr1, bool isMultiLIne, _fdev_put_t *put):
		TextField(canvas, area, font, foreColor, backColor, put)
		{
			SetCharSize(ccr0, ccr1, isMultiLIne);
		}

	//*********************************************************************
	// Public interface
	//*********************************************************************3
public:
	virtual int printf(const char *fmt, ...) __attribute__ ((format (printf, 2, 3)))
	{
		va_list ap;
		int i;

		MakeActive();
		va_start(ap, fmt);
		i = vfprintf(&m_file, fmt, ap);
		va_end(ap);

		return i;
	}

	void SetCharSize(byte ccr0, byte ccr1, bool isMultiLine = false)
	{
		m_ccr0Val = ccr0;
		m_ccr1Val = ccr1;

		m_width = (ccr0 & CCR0_CharHeight_Mask) == CCR0_CharHeight16 ? 8 :
				(ccr0 & CCR0_CharHeight_Mask) == CCR0_CharHeight24 ? 12 : 16;

		m_height = isMultiLine ? m_width * 2 : 0;

		switch (ccr1 & CCR1_CharWidth_Mask)
		{
			case CCR1_CharWidthX1:
				break;

			case CCR1_CharWidthX2:
				m_width *= 2;
				break;

			case CCR1_CharWidthX3:
				m_width *= 3;
				break;

			case CCR1_CharWidthX4:
				m_width *=4;
				break;
		}

		switch (ccr1 & CCR1_CharHeight_Mask)
		{
			case CCR1_CharHeightX1:
				break;

			case CCR1_CharHeightX2:
				m_height *= 2;
				break;

			case CCR1_CharHeightX3:
				m_height *= 3;
				break;

			case CCR1_CharHeightX4:
				m_height *=4;
				break;
		}
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void MakeActive()
	{
		SetupText(m_pCanvas, m_ccr0Val, m_ccr1Val);
		SetForeColor(m_foreColor);
		SetBackColor(m_backColor);
		SetTextPosition(m_curPosX, m_curPosY);
	}

	void WriteCharFixed(byte ch)
	{
		if (ch == '\n')
		{
			m_curPosX = m_pArea->Xpos;
			m_curPosY = m_curPosY + m_height;
			SetTextPosition(m_curPosX, m_curPosY);
			return;
		}
		m_curPosX += m_width;
		Lcd.WriteChar(ch);
	}

	//*********************************************************************
	// instance data
	//*********************************************************************
protected:
	byte	m_ccr0Val;
	byte	m_ccr1Val;
	byte	m_width;
	byte	m_height;
};