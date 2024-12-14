//****************************************************************************
// EepromData.h
//
// Created 11/19/2020 10:44:04 AM by Tim
//
//****************************************************************************

// Null out any macros we aren't using
#ifndef EepromData
#define EepromData(typ, name, ...)
#endif

//*********************************************************************
// EEPROM layout
//
// The size and order of these items should never change so the
// EEPROM in a device remains valid.
//
// EeepromData(typ, name, ...)
//
// typ - standard C++ type
// name - member name in struct
// ... - initial value when EEPROM is not programmed
//
//*********************************************************************

EepromData(TouchInfo, TouchInit, { {}, {}, \
	TouchDefaultMinZ, TouchUpdateRate, TouchInitialDiscard, TouchAverageShift})
// 32-bit aligned here
EepromData(ulong, Brightness, LcdBacklightPwmMax)
// 32-bit aligned here
#define	g_arAxisInfo	(&Eeprom.Data.XaxisInfo) // treat next as array
EepromData(SensorInfo, XaxisInfo, { 1.0, 50, false, false })
EepromData(SensorInfo, YaxisInfo, { 1.0, 50, false, false })
EepromData(SensorInfo, ZaxisInfo, { 1.0, 50, false, false })
EepromData(SensorInfo, QaxisInfo, { 1.0, 50, false, true })	// default to disabled
// 32-bit aligned here
EepromData(bool, fIsMetric, false)
EepromData(byte, OriginNum, 0)
EepromData(bool, fHighlightOffset, true)
EepromData(bool, fToolLenAffectsZ, true)
// 32-bit aligned here
EepromData(double, OldChipLoad, 0)	// replaced by ChipLoad
EepromData(double, Sfm, 0)			// kept in meters/min
// 32-bit aligned here
EepromData(ushort, MaxRpm, 10000)
EepromData(bool, fCncCoordinates, false)
EepromData(bool, fToolLibMetric, false)	// not used
// 32-bit aligned here
EepromData(ushort, Tool, 0)
EepromData(ushort, FlashVersion, 1)
EepromData(long, ChipLoad, 0)
// 32-bit aligned here
EepromData(LatheAssignmentList, LatheAssign, {LATHE_X, LATHE_Z, LATHE_Zprime, LATHE_None})
// 32-bit aligned here
EepromData(double, CompoundAngle, 0)
// 32-bit aligned here
EepromData(bool, fIsLathe, false)
EepromData(bool, fCompoundFactor, true)
EepromData(bool, fLatheRadius, false)
EepromData(bool, fLatheShowT, false)
// 32-bit aligned here
EepromData(ushort, LatheMaxRpm, 10000)

// Undefine all the macros now
#undef	EepromData
