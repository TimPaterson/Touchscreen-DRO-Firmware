//****************************************************************************
// PowerDown.h
//
// Created 3/17/2021 3:11:48 PM by Tim
//
//****************************************************************************

#pragma once


#define DEFAULT_TIME	1, 1, 2024, 12, 0, 0	// month, day, year, hour, minute second


struct PowerDownSave
{
	RtcTime	rtcTime;
	long	arXaxisPos[2];
	long	arYaxisPos[2];
	long	arZaxisPos[2];
	long	arQaxisPos[2];
	long	reserved[7];	// round up to a whole page
};


class PowerDown
{
	static constexpr int FlashRowSize = FLASH_PAGE_SIZE * NVMCTRL_ROW_PAGES;
	static constexpr int SaveRows = ReservedEepromRows;	// from Dro.h
	static constexpr int MaxSaveSpots = SaveRows * FlashRowSize / sizeof(PowerDownSave);
	static constexpr ulong Unprogrammed = 0xFFFFFFFF;

	#define pSaveStart	((PowerDownSave *)NVMCTRL_RWW_EEPROM_ADDR)
	#define pSaveEnd	(&pSaveStart[SaveRows * NVMCTRL_ROW_PAGES])

public:
	inline static bool IsStandby()		{ return s_isStandby; }
		
	static void EnterStandby()
	{
		WDT->CTRL.reg = 0;		// turn off watchdog
		SetBrightnessPwm(0);
		Lcd.StartPowerSave();
		pTouch->WaitTouchRelease();	// block until touch released
		s_isStandby = true;
		EIC->INTFLAG.reg = EI_Touch;	// clear before enabling
		EIC->INTENSET.reg = EI_Touch;
	}
	
	static void ResumeStandby()
	{
		__WFI();
	}
	
	static void LeaveStandby()
	{
		SetBrightnessPwm(Eeprom.Data.Brightness);
		Lcd.EndPowerSave();
		pTouch->WaitTouchRelease();	// block until touch released
		EIC->INTENCLR.reg = EI_Touch;
		s_isStandby = false;
		WDT->CTRL.reg = WDT_CTRL_ENABLE;	// enable watchdog
	}
	
	static void Save() INLINE_ATTR
	{
		PowerDownSave	&save = *s_pSaveNext;

		// Fill in data to save
		save.rtcTime.ReadClock();

		SaveAxis(Xpos, save.arXaxisPos);
		SaveAxis(Ypos, save.arYaxisPos);
		SaveAxis(Zpos, save.arZaxisPos);
		SaveAxis(Qpos, save.arQaxisPos);
		Nvm::WriteRwweePageReady();
	}

	static RtcTime Restore()
	{
		PowerDownSave	*pSaveCur;
		PowerDownSave	*pSave = 0;
		RtcTime			time = 0;

		// Find the last save block
		pSaveCur = pSaveStart;
		for (int i = 0; i < MaxSaveSpots; i++, pSaveCur++)
		{
			if (pSaveCur->rtcTime == Unprogrammed)
			{
				if (s_pSaveNext == NULL)
					s_pSaveNext = pSaveCur;
				continue;
			}

			if (pSaveCur->rtcTime > time)
			{
				time = pSaveCur->rtcTime;
				pSave = pSaveCur;
			}
		}

		if (time != 0)
		{
			RestoreAxis(Xpos, pSave->arXaxisPos);
			RestoreAxis(Ypos, pSave->arYaxisPos);
			RestoreAxis(Zpos, pSave->arZaxisPos);
			RestoreAxis(Qpos, pSave->arQaxisPos);

			pSave++;	// most likely next save location
			if (pSave >= pSaveEnd)
				pSave = pSaveStart;
		}
		else
		{
			pSave = pSaveStart;
			// Default time
			time.SetTime(DEFAULT_TIME);
		}

		// Make sure we're ready for next save
		if (s_pSaveNext == NULL)
		{
			// No erased locations, start at next row
			pSave = (PowerDownSave *)(((ulong)pSave + FlashRowSize - 1) & ~(FlashRowSize - 1));
			s_pSaveNext = pSave;
			Nvm::EraseRwweeRowReady(pSave);
		}

		return time;
	}
	
	static void EraseAllSaved()
	{
		Nvm::EraseRwweeRowReady(pSaveStart);
		Nvm::EraseRwweeRow(ADDOFFSET(pSaveStart, FlashRowSize));
	}

protected:
	static void SaveAxis(AxisPos &axis, long *arPos)
	{
		ulong	pos;

		pos = axis.GetSavePos();
		arPos[0] = axis.GetOrigin(0) + pos;
		arPos[1] = axis.GetOrigin(1) + pos;
	}

	static void RestoreAxis(AxisPos &axis, long *arPos)
	{
		axis.SetOrigin(0, arPos[0]);
		axis.SetOrigin(1, arPos[1]);
	}

	inline static PowerDownSave	*s_pSaveNext;
	inline static byte			s_isStandby;
};
