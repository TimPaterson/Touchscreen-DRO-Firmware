//****************************************************************************
// Dro.cpp
//
// Created 10/1/2020 12:36:41 PM by Tim
//
//****************************************************************************

#include <standard.h>
#include "Dro.h"
#include "PosSensor.h"
#include "LcdDef.h"
#include "RA8876.h"
#include "FatFileSd.h"
#include "UsbDro.h"
#include "AxisDisplay.h"
#include "Actions.h"
#include "FileOperations.h"
#include "VersionUpdate.h"
#include "PowerDown.h"
#include "GT9271.h"

//*********************************************************************
// References

// DroInit.cpp
void StartClock();
void Init();

// Generated bitmap files
extern "C"
{
	extern const byte TargetCursor[256];
	extern const byte PointerCursor[256];
}

//*********************************************************************
// Static data
//*********************************************************************

//*********************************************************************
// Image descriptors

// "Canvas" images
#define START_SCREEN(name)		TouchCanvas name(
#define IMAGE_ADDRESS(val)		val + RamScreenStart,
#define IMAGE_WIDTH(val)		val,
#define IMAGE_HEIGHT(val)		val,
#define IMAGE_STRIDE(val)		val,
#define IMAGE_DEPTH(val)		val,
#define END_SCREEN(name)		&name##HotspotList);

// "ColorImage" images
#define START_SCREEN_Overlay(name)	const ColorImage name = {
#define IMAGE_ADDRESS_Overlay(val)	val + RamScreenStart,
#define IMAGE_STRIDE_Overlay(val)	val,
#define IMAGE_DEPTH_Overlay(val)	val
#define END_SCREEN_Overlay(name)	};

#include "Images/Screen.h"

// Areas
#define START_AREAS(name)					const name##_Areas_t name##_Areas = {
#define DEFINE_AREA(name, x1, y1, x2, y2)	{x1, y1, x2, y2},
#define END_AREAS(name)						};

#include "Images/Screen.h"

//*********************************************************************
// Component data

VersionInfo_t VersionInfo VERSION_INFO_ATTR = { DroFirmwareId, PROGRAM_VERSION, GRAPHICS_VERSION, FONT_VERSION };

Console_t	Console;
FILE		Console_FILE;
FDEV_STANDARD_STREAMS(&Console_FILE, NULL);

Xtp2046		ResTouch;
Gt9271		CapTouch;
TouchMgr	NoTouch;
TouchMgr*	pTouch = &NoTouch;
UsbDro		UsbPort;
FileBrowser	Files;
ToolLib		Tools;

FatSd			Sd;
FatSys			FileSys;
FileOperations	FileOp;
FAT_DRIVES_LIST(&FlashDrive, &Sd);

//********************************************************************
// Define the four sensors

static const int AxisUpdateRate = 20;	// updates per second
static const int FeedUpdateRate = 8;	// updates per second
static const int MinBrightness = LcdBacklightPwmMax * 10 / 100;	// min = 10%

AxisPos Qpos(&Eeprom.Data.QaxisInfo);
AxisPos Xpos(&Eeprom.Data.XaxisInfo);
AxisPos Ypos(&Eeprom.Data.YaxisInfo);
AxisPos Zpos(&Eeprom.Data.ZaxisInfo, &Qpos);

AxisDisplay Xdisplay(MainScreen_Areas.Xdisplay, MainScreen_Areas.UndoX1, 
	MainScreen_Areas.Xbutton, MainScreen_Areas.UndoLabelX);
AxisDisplay Ydisplay(MainScreen_Areas.Ydisplay, MainScreen_Areas.UndoY1, 
	MainScreen_Areas.Ybutton, MainScreen_Areas.UndoLabelY);
AxisDisplay Zdisplay(MainScreen_Areas.Zdisplay, MainScreen_Areas.UndoZ1, 
	MainScreen_Areas.Zbutton, MainScreen_Areas.UndoLabelZ);

//********************************************************************
// EEPROM data

// Define initial EEPROM data
#define EepromData(typ, name, ...)	__VA_ARGS__,

const Eeprom_t RwwData =
{
	#include "EepromData.h"
};

// Create an EepromMgr for it, which includes a copy in RAM
EepromMgr_t Eeprom;

//*********************************************************************
// Tests
//*********************************************************************

void NO_INLINE_ATTR HardFault(int *p)
{
	*p = 0;
}

//*********************************************************************
// Helpers
//*********************************************************************

void ChangeScreenBrightness(int change)
{
	change = LcdBacklightPwmMax * change / 100;	// change was %
	change += Eeprom.Data.Brightness;
	if (change < MinBrightness)
		change = MinBrightness;
	if (change > LcdBacklightPwmMax)
		change = LcdBacklightPwmMax;
	Eeprom.Data.Brightness = change;
	SetBrightnessPwm(change);
}

FatDateTime GetFatTime()
{
	FatDateTime	dt;
	RtcTime		time;

	dt.ul = time.ReadClock().GetFatTime();
	return dt;
}

void PrintHelp()
{
	printf("Commands:\n"
		"f - Load fonts from USB file Fonts.bin\n"
		"i - Load images from USB file Screen.bin\n"
		"p - Toggle test pattern\n"
		"t - Calibrate touchscreen\n"
		"x - Reset\n"
	);
}

//*********************************************************************
// Main program
//*********************************************************************

int main(void)
{
	RtcTime	timeCur, timeSave;
	bool	lcdPresent;
	bool	isTestPattern = false;

	StartClock();
	Init();
	Timer::Init();
	Eeprom.Init();
	RtcTime::Init();

	Console.Init(RXPAD_Pad1, TXPAD_Pad2);
	Console.SetBaudRate(CONSOLE_BAUD_RATE);
	Console.StreamInit(&Console_FILE);
	Console.Enable();

	printf("\nDRO version " STRINGIFY(PROGRAM_VERSION) "\n");
	
	if (Eeprom.Data.FlashVersion != FLASH_VERSION)
	{
		// Tool library and/or power down save flash format changed.
		// Erase them.
		DEBUG_PRINT("Erasing libraries\n");
		
		PowerDown::EraseAllSaved();
		
		Eeprom.Data.FlashVersion = FLASH_VERSION;
		Eeprom.StartSave();
	}

	timeCur.ReadClock();
	timeSave = PowerDown::Restore();
	if (timeSave > timeCur)
		timeSave.SetClock();
		
	if (PM->RCAUSE.reg & PM_RCAUSE_WDT)
	{
		DEBUG_PRINT("WDT Reset\n");
	}

	// Put EEPROM data into effect
	SetBrightnessPwm(Eeprom.Data.Brightness);
	Xpos.SensorInfoUpdate();
	Ypos.SensorInfoUpdate();
	Zpos.SensorInfoUpdate();
	Qpos.SensorInfoUpdate();

	// Initialize USB
	Mouse.Init(Lcd.ScreenWidth, Lcd.ScreenHeight);
	UsbPort.Init();
	UsbPort.Enable();

	// Initialize file system
	Sd.SpiInit(SPIMISOPAD_Pad1, SPIOUTPAD_Pad2_MOSI_Pad3_SCK);
	Sd.Enable();
	FileSys.Init();

	// Initialize LCD and touch if present
	lcdPresent = Lcd.Init();
	if (lcdPresent)
	{
		// Copy serial data in graphics memory
		Lcd.CopySerialMemToRam(FlashScreenStart, RamScreenStart, ScreenFileLength, SerialFlashPort);
		Lcd.CopySerialMemToRam(FlashFontStart, RamFontStart, FontFileLength, SerialFlashPort);

		Lcd.LoadGraphicsCursor(PointerCursor, GTCCR_GraphicCursorSelect1);
		Lcd.LoadGraphicsCursor(TargetCursor, GTCCR_GraphicCursorSelect2);
		Lcd.SetGraphicsCursorColors(0xFF, 0x00);

		Lcd.SetMainImage(&MainScreen);
		Lcd.DisplayOn();

		// Search for touch panel
		for ( int i = 0; ; i++)
		{
			if (CapTouch.Init())
			{
				pTouch = &CapTouch;
				DEBUG_PRINT("Found CTP\n");
				break;
			}

			if (ResTouch.Init(SPIMISOPAD_Pad3, SPIOUTPAD_Pad0_MOSI_Pad1_SCK))
			{
				pTouch = &ResTouch;
				DEBUG_PRINT("Found RTP\n");
				break;
			}

			DEBUG_PRINT("No touch panel found\n");

			if (i == 3)
			{
				// No touch panel, put up error message box. Mouse can still be used.
				Lcd.EnablePip1(&NoTouchPanel, NoTouchLeft, NoTouchTop, true);
				// Leave pTouch set to empty touch panel
				break;
			}
		}

		// Initialize touch panel
		TouchMgr::SetSize(Lcd.ScreenWidth, Lcd.ScreenHeight);
		if (!TouchMgr::SetMatrix(&Eeprom.Data.TouchInit))
		{
			// touch panel not calibrated
			if (pTouch == &NoTouch)
			{
				// UNDONE: No touch panel, require mouse
			}
			else
			{
				TouchCalibrate::Open();
			}
		}
		
		Actions::Init();

		DEBUG_PRINT("Graphics memory allocated: %lu bytes\n", Canvas::AllocVideoRam(0));

		PrintHelp();
	}

	// Start WDT now that initialization is complete
	WDT->CTRL.reg = WDT_CTRL_ENABLE;

	// Finally, enable NMI
	EIC->NMICTRL.reg = EIC_NMICTRL_NMISENSE_FALL | EIC_NMICTRL_NMIFILTEN;

	//************************************************************************
	// Main loop

	Timer	tmrAxis;
	Timer	tmrFeed;
	int		i;
	bool	fSdOut = true;
	RtcTime	timeLast{true};

	tmrFeed.Start(tmrAxis.Start());

    while (1)
    {
		while(PowerDown::IsStandby())
			PowerDown::ResumeStandby();

		wdt_reset();

		// Process EEPROM save if in progress
		if (!Eeprom.Process())
			PowerDown::Process();

		// Check status of SD card
		// UNDONE: disable SD while developing next version of PCB
		if (false && !GetSdCd() == fSdOut && !FileOp.IsBusy())
		{
			fSdOut = !fSdOut;
			if (fSdOut)
			{
				Sd.Dismount();
				DEBUG_PRINT("SD card dismounted\n");
			}
			else
			{
				FileOp.Mount(Sd.GetDrive());
				DEBUG_PRINT("SD card mounting...");
			}
		}

		if (timeCur.ReadClock() != timeLast)
		{
			timeLast = timeCur;
			Tools.ShowExportTime(timeCur);
		}

		// Update the axis position displays
		if (tmrAxis.CheckInterval_rate(AxisUpdateRate))
			AxisDisplay::UpdateAll();

		// Update the current feed rate
		if (tmrFeed.CheckInterval_rate(FeedUpdateRate))
		{
			double	deltaX, deltaY, deltaZ, delta;

			deltaX = Xpos.GetDistance();
			deltaY = Ypos.GetDistance();
			deltaZ = Zpos.GetDistance();
			delta = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
			// convert to per minute
			Tools.ShowFeedRate(delta * 60.0 * FeedUpdateRate);
		}

		// Process USB events
		i = UsbPort.Process();
		if (i != HOSTACT_None)
		{
			int	X, Y;
			uint flags;
			ButtonState buttons;

			switch (i)
			{
			case HOSTACT_MouseChange:
				X = Mouse.GetX();
				Y = Mouse.GetY();
				Lcd.SetGraphicsCursorPosition(X, Y);

				buttons = Mouse.GetButtons();
				if (buttons.btnStart & BUTTON_Left)
					flags = TOUCH_Start | TOUCH_Touched;
				else if (buttons.btnDown & BUTTON_Left)
					flags = TOUCH_Touched;
				else if (buttons.btnEnd & BUTTON_Left)
					flags = TOUCH_End;
				else
					flags = TOUCH_None;

				if (flags != TOUCH_None)
					Actions::TakeAction(X, Y, flags);
				break;

			case HOSTACT_FlashReady:
				FileOp.Mount(FlashDrive.GetDrive());
				DEBUG_PRINT("USB drive mounting...");
				break;

			case HOSTACT_KeyboardChange:
				KeyboardMgr::UsbKeyHit(Keyboard.GetKeyByte());
				break;

			case HOSTACT_AddDevice:
				if (Mouse.IsLoaded())
				{
					Lcd.EnableGraphicsCursor(GTCCR_GraphicCursorSelect1);
					Mouse.SetPos(Lcd.ScreenWidth / 2, Lcd.ScreenHeight / 2);
					Lcd.SetGraphicsCursorPosition(Lcd.ScreenWidth / 2, Lcd.ScreenHeight / 2);
				}
				break;

			case HOSTACT_RemoveDevice:
				if (FlashDrive.IsMounted())
				{
					FlashDrive.Dismount();
					DEBUG_PRINT("USB drive dismounted\n");
				}

				// Turn mouse off
				Lcd.DisableGraphicsCursor();
				break;
			}
		}

		// Process file operations
		FileOp.Process();

		// Process screen touch
		if (lcdPresent && pTouch->Process())
		{
			uint	flags;

			// Touch sensor has an update
			flags = pTouch->GetTouch();
			if (flags != TOUCH_None)
			{
				int	x, y;

				x = pTouch->GetX();
				y = pTouch->GetY();

				Actions::TakeAction(x, y, flags);
			}
		}

		if (Console.IsByteReady())
		{
			byte	ch;
			int		err;

			ch = Console.ReadByte();
			if (ch >= 'A' && ch <= 'Z')
				ch += 'a' - 'A';

			switch (ch)
			{
				// Use lower-case alphabetical order to find the right letter
			case 'f':
				printf("Loading font...");
				err = FileOp.WriteFileToFlash("Fonts.bin", FlashFontStart);
FileErrChk:
				if (err < 0)
					printf("file operation failed with code %i\n", err);
				break;

			case 'i':
				printf("Loading image...");
				err = FileOp.WriteFileToFlash("Screen.bin", FlashScreenStart);
				goto FileErrChk;

			case 'p':
				if (lcdPresent)
				{
					if (isTestPattern)
					{
						printf("Test pattern off\n");
						Lcd.DisplayOn();	// turn off pattern
						isTestPattern = false;
					}
					else
					{
						printf("Test pattern on\n");
						Lcd.TestPattern();
						isTestPattern = true;
					}
				}
				break;

			case 't':
				if (lcdPresent)
					TouchCalibrate::Open();
				break;

			case 'x':
				HardFault((int *)&g_FileBuf[0][1]);
				break;

			default:
				if (lcdPresent)
					PrintHelp();
				break;
			}
		}
    }
}
