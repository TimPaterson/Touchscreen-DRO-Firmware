//****************************************************************************
// FontInfo.c
//
// Created 11/29/2020 10:21:49 AM by Tim
//
//****************************************************************************

#include <standard.h>
#include "FontInfo.h"
#include "SerialMem.h"


//****************************************************************************
// Define fonts using macros generated by FontGenerator

#define START_FONT(name)		const FontInfo FONT_##name = {
#define END_FONT(name)			};

#define FONT_START_OFFSET(val)	.FontStart = val + RamFontStart,
#define CHARSET_WIDTH(val)		.CharsetWidth = val,
#define CHAR_HEIGHT(val)		.Height = val,
#define FIRST_CHAR(val)			.FirstChar = val,
#define LAST_CHAR(val)			.LastChar = val,
#define CHAR_STRIDE(val)		.CharStride = val,
#define START_CHAR_WIDTHS(name)	{
#define CHAR_WIDTH(val)			val,
#define END_CHAR_WIDTHS(name)	}

// Run the macros
#include "Fonts/Fonts.h"
