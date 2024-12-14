//****************************************************************************
// Interrupts.cpp
//
// Created 10/13/2020 12:32:39 PM by Tim
//
//****************************************************************************

#include <standard.h>
#include "Dro.h"
#include "PowerDown.h"
#include "GT9271.h"


//****************************************************************************
// USART

DEFINE_USART_ISR(SERCOM0, Console)

DEFINE_I2C_ISR(SERCOM1, CapTouch)

//****************************************************************************
// Power down

void NonMaskableInt_Handler()
{
	SET_TP;

	EIC->NMIFLAG.reg = EIC_NMIFLAG_NMI;
	// disable NMI until we're done with EEPROM
	EIC->NMICTRL.reg = EIC_NMICTRL_NMISENSE_NONE;
	
	PowerDown::Save();
	CLEAR_TP;
}

//****************************************************************************
// External Interrupts

void EIC_Handler()
{
	uint	uIntFlags;
	uint	uPortVal;

	uIntFlags = EIC->INTFLAG.reg;
	EIC->INTFLAG.reg = uIntFlags;	// reset all flags

	// Is it a position sensor?
	if (uIntFlags & PosSensorIrqMask)
	{
		uPortVal = PORTA->IN.ul;
		Xpos.InputChange(uPortVal >> XposA_BIT);
		Ypos.InputChange(uPortVal >> YposA_BIT);
		Zpos.InputChange(uPortVal >> ZposA_BIT);
		Qpos.InputChange(uPortVal >> QposA_BIT);
	}
	
	// Touchscreen?
	if (PowerDown::IsStandby() && (uIntFlags & EI_Touch) && pTouch->CheckLeaveStandby())
		PowerDown::LeaveStandby();
}

//****************************************************************************
// WDT

#ifdef DEBUG

EXTERN_C void WdtHelper(uint uAddr)
{
	WDT->INTFLAG.reg = WDT_INTFLAG_EW;	// Clear interrupt
	DEBUG_PRINT("\nWDT interrupt from %x\n", uAddr);
}

void NAKED_ATTR WDT_Handler()
{
	asm volatile (
		"ldr	r0, [sp, #0x18] \n\t"
		"b		WdtHelper \n\t"
	);
}

#endif

//****************************************************************************
// Hard Fault

#ifdef DEBUG

EXTERN_C void WaitLoop()
{
	while (1);
}

EXTERN_C void HardFaultHelper(void * *ppv)
{
	void	*pv;
	
	pv = *ppv;
	*ppv = (void *)WaitLoop;
	DEBUG_PRINT("\nHard Fault at %p\n", pv);
}

void NAKED_ATTR HardFault_Handler()
{
	asm volatile (
		"mov	r0, sp \n\t"
		"add	r0, #0x18 \n\t"
		"b		HardFaultHelper \n\t"
	);
}

#endif
