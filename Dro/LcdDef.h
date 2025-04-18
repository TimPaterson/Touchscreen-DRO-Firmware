//****************************************************************************
// LcdDef.h
//
// Created 10/24/2020 12:18:34 PM by Tim
//
//****************************************************************************

#pragma once

#include "RA8876const.h"


using namespace RA8876const;

// Define this symbol only if the RA8876 has a 16-bit MCU interface
#define RA8876_16BIT_BUS


//*************************************************************************
// This class is required as a base for the generic RA8876 class. It
// defines the hardware interface and initialization. Any member declared 
// protected or public is required for the derived classes.
//

class RA8876_Base
{
	//*********************************************************************
	// Types

	struct RegValue
	{
		byte	addr;
		byte	val;
	};

	//*********************************************************************
	// Initialization values for East Rising ER-TFTM101-1
	//*********************************************************************
public:
	static constexpr int ScreenWidth = 1024;
	static constexpr int ScreenHeight = 600;

protected:
	// Hardware settings
	static constexpr long CoreFreq = 100'000'000;	// RA8876 core clock - CCLK
private:
	static constexpr long OscFreq = 10'000'000;		// Crystal clock
	static constexpr long SdramFreq = 100'000'000;	// SDRAM clock - MCLK
	static constexpr int SdramRefresh = 64;			// SDRAM refresh interval; ms
	static constexpr int SdramRowSize = 8192;		// SDRAM row size (for refresh)
	static constexpr int SdramRefInterval = SdramFreq / 1000 * SdramRefresh / SdramRowSize - 2;

	// LCD parameters
	static constexpr long LcdScanFreq = 50'000'000;	// LCD scan clock - SCLK
	
	// BuyDisplay.com has code samples with different values. The 10" display
	// needs the sync times.
#if 1
	// HSync
	static constexpr int LcdHsyncWidthPx = 70;
	static constexpr int LcdHsyncFrontPorchPx = 160;
	static constexpr int LcdHsyncBackPorchPx = 160;
	// VSync
	static constexpr int LcdVsyncWidthLn = 10;
	static constexpr int LcdVsyncFrontPorchLn = 12;
	static constexpr int LcdVsyncBackPorchLn = 23;
#else
	// HSync
	static constexpr int LcdHsyncWidthPx = 20;
	static constexpr int LcdHsyncFrontPorchPx = 160;
	static constexpr int LcdHsyncBackPorchPx = 140;
	// VSync
	static constexpr int LcdVsyncWidthLn = 3;
	static constexpr int LcdVsyncFrontPorchLn = 12;
	static constexpr int LcdVsyncBackPorchLn = 20;
#endif

	// Serial flash/ROM SPI settings
	// Unit 1: serial flash
	static constexpr long MaxSpiClock1 = 30'000'000;

	// Unit 0: not installed, set the same to avoid clock speed switching
	static constexpr long MaxSpiClock0 = MaxSpiClock1;

	enum RA8876_InitValues
	{
		// Chip Configuration Registers
		CCR_Init = CCR_BusWidth16 | CCR_SpiEnable | CCR_I2Cdisable | CCR_LcdWidth24 |
			CCR_KeyScanDisable | CCR_WaitMaskOff,

		// PLL Initialization values
		// VCO has range 250 - 500 MHz. Dividing output by 4 will gives
		// range 62.5 - 125 MHz for MCLK and CCLK. Divide by 8 gives 
		// 31.25 - 62.5 MHz for SCLK.
		
		RA_MPLLC1_Init = PLL_Postscale4,
		RA_MPLLC2_Init = SdramFreq * 4 / OscFreq - 1,

		RA_CPLLC1_Init = PLL_Postscale4,
		RA_CPLLC2_Init = CoreFreq * 4 / OscFreq - 1,

		RA_SPLLC1_Init = PLL_Postscale8,
		RA_SPLLC2_Init = LcdScanFreq * 8 / OscFreq - 1,

		// SDRAM Initialization values
		SDRAR_Init = SDRAR_ColumnBits9 | SDRAR_RowBits12 | SDRAR_Banks4 | SDRAR_RefreshPowerDown,
		SDRMD_Init = SDRMD_CasClocks3 | SDRMD_DriverFull | SDRMD_ArrayFull,
		SDRCR_Init = SDRCR_InitDone | SDRCR_ClearWarning | SDRCR_BusWidth16 | SDRCR_BurstSize256,

		// Display Initialization
		MACR_Init = MACR_LeftRightTopBottom | MACR_MaskNone,
		ICR_Init = ICR_MemPortSdram | ICR_IrqLevelHigh | ICR_GraphicsMode,
		DPCR_Init = DPCR_OutputSequenceRGB | DPCR_VertScanTopToBottom | DPCR_PclkEdgeFalling,
		PCSR_Init = PCSR_VsyncIdleHigh | PCSR_HsyncIdleHigh | PCSR_DataIdleLow |
			PCSR_ClockIdleLow | PCSR_DataEnableIdleLow | PCSR_DataEnableActiveHigh |
			PCSR_VsyncActiveHigh | PCSR_HsyncActiveHigh,
		HDWR_Init = ScreenWidth / 8 - 1,
		HDWFTR_Init = ScreenWidth % 8,
		HNDR_Init = LcdHsyncBackPorchPx / 8 - 1,
		HNDFTR_Init = LcdHsyncBackPorchPx % 8,
		HSTR_Init = LcdHsyncFrontPorchPx / 8 - 1,
		HPWR_Init = (LcdHsyncWidthPx - 1) / 8,
		VDHR0_Init = (ScreenHeight - 1) & 0xFF,
		VDHR1_Init = (ScreenHeight - 1) >> 8,
		VNDR0_Init = (LcdVsyncBackPorchLn - 1) & 0xFF,
		VNDR1_Init = (LcdVsyncBackPorchLn - 1) >> 8,
		VSTR_Init = LcdVsyncFrontPorchLn - 1,
		VPWR_Init = LcdVsyncWidthLn - 1,
		MPWCTR_Init = MPWCTR_SyncEnable,

		// SPI Initialization
		// SPI Serial character ROM at CS0

		// SPI Serial flash at CS1
		SPIMCR_Init = SPIMCR_SpiMode0 | SPIMCR_EmtIrqMasked |
			SPIMCR_OvfIrqMasked | SPIMCR_SlaveSelectCs1 | SPIMCR_IrqEnable,
	};

	enum LT7683_InitValues
	{
		// Use same PLL values as RA8876, but they are set differently.
		// SCLK is called PCLK on LT7683.
		
		LT_DivRatioPos = 1,		// position in control register 1 of divider ratio
		
		LT_MPLLC1_Init = 4 << LT_DivRatioPos,
		LT_MPLLC2_Init = SdramFreq * 4 / OscFreq,

		LT_CPLLC1_Init = 4 << LT_DivRatioPos,
		LT_CPLLC2_Init = CoreFreq * 4 / OscFreq,

		LT_SPLLC1_Init = 8 << LT_DivRatioPos,
		LT_SPLLC2_Init = LcdScanFreq * 8 / OscFreq,
	};
	
	//*********************************************************************
	// Serial memory setup needed in class RA8876
	//*********************************************************************
protected:
	static constexpr long SpiClock0 = std::min(MaxSpiClock0, CoreFreq / 2);
	static constexpr int SpiDivisor0 = (CoreFreq / 2 + SpiClock0 - 1) / SpiClock0 - 1;
	static constexpr int SFL_CTRL_Init0 = SFL_CTRL_Select0 | SFL_CTRL_ReadCommand0B | SFL_CTRL_AddrBits24;

	static constexpr long SpiClock1 = std::min(MaxSpiClock1, CoreFreq / 2);
	static constexpr int SpiDivisor1 = (CoreFreq / 2 + SpiClock1 - 1) / SpiClock1 - 1;
	static constexpr int SFL_CTRL_Init1 = SFL_CTRL_Select1 | SFL_CTRL_ReadCommand3B | SFL_CTRL_AddrBits24;

	//*********************************************************************
	// Hardware-specific I/O needed in class RA8876
	//*********************************************************************
protected:
	static uint GetStatus() NO_INLINE_ATTR
	{
		uint	status;

		PORTB->DIR.Lcd16 = 0;	// Switch to inputs
		SetLcdPin(LcdRW);
		ClearLcdPin(LcdCs | LcdCD);
		SetLcdPin(LcdE);	// toggle E
		Timer::ShortDelay_clocks(2);
		status = PORTB->IN.Lcd8;
		ClearLcdPin(LcdE);
		SetLcdPin(LcdCs);
		PORTB->DIR.Lcd16 = LcdData16;	// Switch back to outputs
		return status;
	}

	static void WriteAddrInline(uint addr) INLINE_ATTR
	{
		// Write address
		ClearLcdPinInline(LcdRW | LcdCs | LcdCD);
		PORTB->OUT.Lcd8 = addr;	// Address
		ToggleEnableInline();
		SetLcdPinInline(LcdCD | LcdCs);
	}

public:
	static void WriteDataInline(uint val) INLINE_ATTR
	{
		// Write data
		SetLcdPinInline(LcdCD);
		ClearLcdPinInline(LcdRW | LcdCs);
		PORTB->OUT.Lcd8 = val;	// Data
		ToggleEnableInline();
		SetLcdPinInline(LcdCs);
	}

	static uint ReadDataInline() INLINE_ATTR
	{
		uint	val;

		// Read data
		PORTB->DIR.Lcd16 = 0;	// Switch to inputs
		ClearLcdPinInline(LcdCs);
		SetLcdPinInline(LcdCD | LcdRW);
		SetLcdPinInline(LcdE);	// toggle E
		Timer::ShortDelay_clocks(2);
		val = PORTB->IN.Lcd8;
		ClearLcdPinInline(LcdE);
		SetLcdPinInline(LcdCs);
		PORTB->DIR.Lcd16 = LcdData16;	// Switch back to outputs
		return val;
	}

#ifdef RA8876_16BIT_BUS

	static void WriteData16Inline(uint val) INLINE_ATTR
	{
		// Write data
		SetLcdPinInline(LcdCD);
		ClearLcdPinInline(LcdRW | LcdCs);
		PORTB->OUT.Lcd16 = val;	// Data
		ToggleEnableInline();
		SetLcdPinInline(LcdCs);
	}

	// This must be able to fully inline so it can be in RAM for 
	// programming flash
	static uint ReadData16Inline() INLINE_ATTR
	{
		uint	val;

		// Read data
		PORTB->DIR.Lcd16 = 0;	// Switch to inputs
		ClearLcdPinInline(LcdCs);
		SetLcdPinInline(LcdCD | LcdRW);
		SetLcdPinInline(LcdE);	// toggle E
		Timer::ShortDelay_clocks(2);
		val = PORTB->IN.Lcd16;
		ClearLcdPinInline(LcdE);
		SetLcdPinInline(LcdCs);
		PORTB->DIR.Lcd16 = LcdData16;	// Switch back to outputs
		return val;
	}

#endif	// RA8876_16BIT_BUS

	//*********************************************************************

public:
	static void WriteRegInline(uint addr, uint val) INLINE_ATTR
	{
		WriteAddrInline(addr);
		WriteDataInline(val);
	}

	static uint ReadRegInline(uint addr) INLINE_ATTR
	{
		WriteAddrInline(addr);
		return ReadDataInline();
	}

	static void WriteReg(uint addr, uint val)
	{
		WriteRegInline(addr, val);
	}

	static uint ReadReg(uint addr)
	{
		return ReadRegInline(addr);
	}

	//*********************************************************************
	// Hardware-specific RA8876 I/O
	//*********************************************************************
private:
	static void ToggleEnableInline() INLINE_ATTR
	{
		SetLcdPinInline(LcdE);	// toggle E
		Timer::ShortDelay_clocks(1);
		ClearLcdPinInline(LcdE);
	}

	// When the RA8876 is first powered on, it runs on the crystal clock
	// at only 10 MHz. Make sure it's ready by checking the WAIT line.
	static void WriteRegSlow(uint addr, uint val)
	{
		while (GetLcdWait() == 0);
		WriteAddrInline(addr);
		Timer::ShortDelay_clocks(1);	// Pause for WAIT to assert
		while (GetLcdWait() == 0);
		WriteDataInline(val);
	}

	static void WriteRegListSlow(const RegValue *pList, int iLen)
	{
		do
		{
			WriteRegSlow(pList->addr, pList->val);
			pList++;
		} while (--iLen > 0);
	}

	static uint ReadRegSlow(uint addr)
	{
		while (GetLcdWait() == 0);
		WriteAddrInline(addr);
		Timer::ShortDelay_clocks(1);	// Pause for WAIT to assert
		while (GetLcdWait() == 0);
		return ReadDataInline();
	}

	static void WriteRegList(const RegValue *pList, int iLen)
	{
		do
		{
			WriteReg(pList->addr, pList->val);
			pList++;
		} while (--iLen > 0);
	}

	//*********************************************************************

public:
	static bool Init()
	{
		if ((GetStatus() & (STATUS_WriteFifoFull | STATUS_WriteFifoEmpty)) != STATUS_WriteFifoEmpty)
			return false;	// screen not present

		// Make sure we're ready to accept commands
		WriteRegSlow(PMU, 0);	// Make sure not in power-down
		while (GetStatus() & (STATUS_InhibitOperation | STATUS_CoreBusy));

		// Software reset
		WriteRegSlow(SRR, SRR_Reset);
		// Wait for reset to end
		while (GetStatus() & STATUS_InhibitOperation);

		// Initialize PLL
		// PLL initialization differs for RA8876 vs. LT7683
		// We can tell which we have by reading SRR
		if (ReadRegSlow(SRR) == 0)
			WriteRegListSlow(s_arLT7683_PllInitList, _countof(s_arLT7683_PllInitList));
		else
			WriteRegListSlow(s_arRA8876_PllInitList, _countof(s_arRA8876_PllInitList));
		
		// Wait for PLL to stabilize
		while((ReadRegSlow(CCR) & CCR_PllReconfigure) == 0);

		// Initialize SDRAM
		WriteRegList(s_arSdramInitList, _countof(s_arSdramInitList));
		// Wait for SDRAM to be ready
		while ((GetStatus() & STATUS_SdramReady) == 0);

		// Initialize LCD
		WriteRegList(s_arInitList, _countof(s_arInitList));

		// Let caller know screen is present
		return true;
	}

	//*********************************************************************
	// Function-specific handlers

	static void TestPattern()
	{
		WriteReg(DPCR, DPCR_Init | DPCR_DisplayOn | DPCR_DisplayTestBar);
	}

	static void DisplayOn()
	{
		WriteReg(DPCR, DPCR_Init | DPCR_DisplayOn);
	}

	static void DisplayOff()
	{
		WriteReg(DPCR, DPCR_Init | DPCR_DisplayOff);
	}

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const RegValue s_arRA8876_PllInitList[] = {
		// Initialize PLL
		SPPLLC1, RA_SPLLC1_Init,
		SPPLLC2, RA_SPLLC2_Init,
		MPPLLC1, RA_MPLLC1_Init,
		MPPLLC2, RA_MPLLC2_Init,
		CPPLLC1, RA_CPLLC1_Init,
		CPPLLC2, RA_CPLLC2_Init,
		CCR, CCR_Init,
		// Also write to CCR to reconfigure PLL
		CCR, CCR_Init | CCR_PllReconfigure,
	};

	inline static const RegValue s_arLT7683_PllInitList[] = {
		// Initialize PLL
		SPPLLC1, LT_SPLLC1_Init,
		SPPLLC2, LT_SPLLC2_Init,
		MPPLLC1, LT_MPLLC1_Init,
		MPPLLC2, LT_MPLLC2_Init,
		CPPLLC1, LT_CPLLC1_Init,
		CPPLLC2, LT_CPLLC2_Init,
		CCR, CCR_Init,
		// Also write to SSR to reconfigure PLL
		SRR, 0,
		SRR, CCR_PllReconfigure,
	};

	inline static const RegValue s_arSdramInitList[] = {
		// Initialize SDRAM
		SDRAR, SDRAR_Init,
		SDRMD, SDRMD_Init,
		SDR_REF_ITVL0, (byte)SdramRefInterval,
		SDR_REF_ITVL1, SdramRefInterval >> 8,
		SDRCR, SDRCR_Init,
	};

	inline static const RegValue s_arInitList[] = {
		// Initialize Display
		MACR,	MACR_Init,
		ICR,	ICR_Init,
		PCSR,	PCSR_Init,
		HDWR,	HDWR_Init,
		HDWFTR,	HDWFTR_Init,
		HNDR,	HNDR_Init,
		HNDFTR,	HNDFTR_Init,
		HSTR,	HSTR_Init,
		HPWR,	HPWR_Init,
		VDHR0,	VDHR0_Init,
		VDHR1,	VDHR1_Init,
		VNDR0,	VNDR0_Init,
		VNDR1,	VNDR1_Init,
		VSTR,	VSTR_Init,
		VPWR,	VPWR_Init,
		MPWCTR,	MPWCTR_Init,
		GTFNT_SEL, GTFNT_SEL_GT30L32S4W,
		DPCR,	DPCR_Init,
		SPI_DIVSOR, SpiDivisor0,
	};
};
