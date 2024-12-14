//****************************************************************************
// LoadGraphics.h
//
// Created 12/7/2024 3:45:04 PM by tp-ms
//
//****************************************************************************

#pragma once


class LoadGraphics : public FileBrowser
{
	// Text messages
	#define FIRMWARE_LABEL	"Current Firmware:"
	#define GRAPHICS_LABEL	"Graphics:"
	#define FONTS_LABEL		"Fonts:"
	#define UPDATE_LABEL	"Update:"
	#define INVALID_HEADER	"Not a valid update file."
	#define BANNER_MSG		" Insert flash drive\n"  \
							"with DRO update file\n" \
							"       (.upd)"

	// Banner
	static constexpr int BannerLeft = 150;
	static constexpr int BannerTop = 180;
	static constexpr int BannerWidth = ScreenWidth - 2 * BannerLeft;
	static constexpr int BannerHeight = ScreenHeight - 2 * BannerTop;
	static constexpr int BannerBorderWidth = NoTouchBorderWidth;
	static constexpr int BannerBackColor = ScreenBackColor;
	static constexpr int BannerBorderColor = NoTouchBorderColor;
	static constexpr int BannerTextLeft = BannerLeft + 45;
	static constexpr int BannerTextTop = BannerTop + 20;
	static constexpr int BannerTextColor = 0;

	// Program button
	static constexpr int ProgBtnLeft = UpdateButtonLeft;
	static constexpr int ProgBtnTop = UpdateButtonTop;
	static constexpr int ProgBtnWidth = UpdateButtonWidth * 2;
	static constexpr int ProgBtnHeight = UpdateButtonHeight;
	static constexpr int ProgBtnTextLeft = ProgBtnLeft + 5;
	static constexpr int ProgBtnTextTop = ProgBtnTop + 25;
	static constexpr int ProgBtnTextWidth = ProgBtnWidth - 10;
	static constexpr int ProgBtnTextHeight = ProgBtnHeight - 50;
	static constexpr int ProgBtnColor = 0x60ff00;
	static constexpr int ProgBtnTextColor = 0;

	static constexpr int FileIconSize = 32;
	// icon 0: white rectangle (sheet of paper) for file
	static constexpr int Icon0_X = FileIconSize * 0;
	static constexpr int SheetX = 4 + Icon0_X;
	static constexpr int SheetY = 1;
	static constexpr int SheetWidth = FileIconSize - 2 * SheetX;
	static constexpr int SheetHeight = FileIconSize - SheetY * 2;
	static constexpr int SheetColor = 0xFFFFFF;	// white

	// icon 1: yellow rectangle for folder
	static constexpr int Icon1_X = FileIconSize * 1;
	static constexpr int FolderX = 1 + Icon1_X;
	static constexpr int FolderY = 5;
	static constexpr int FolderWidth = FileIconSize - 2 * 1;
	static constexpr int FolderHeight = FileIconSize - FolderY - 1;
	static constexpr int FolderColor = 0xFFD700;	// gold

	// icon 2: left arrow, made with triangle and rectangle
	static constexpr int Icon2_X = FileIconSize * 2;
	static constexpr int ArrowWidth = 20;
	static constexpr int ArrowPointX = 0 + Icon2_X;
	static constexpr int ArrowPointY = FileIconSize / 2;
	static constexpr int ArrowTopX = ArrowWidth + Icon2_X;
	static constexpr int ArrowTopY = 2;
	static constexpr int ArrowBottomX = ArrowTopX;
	static constexpr int ArrowBottomY = ArrowPointY + (ArrowPointY - ArrowTopY);
	static constexpr int ArrowBodyX = ArrowTopX;
	static constexpr int ArrowBodyWidth = FileIconSize - ArrowWidth;
	static constexpr int ArrowBodyY = 10;
	static constexpr int ArrowBodyHeight = FileIconSize - ArrowBodyY * 2;
	static constexpr int ArrowColor = 0xFFA500;	// orange

	// icon 3: red circle for error
	static constexpr int Icon3_X = FileIconSize * 3;
	static constexpr int CircleRadius = FileIconSize / 2 - 2;
	static constexpr int CircleCenterX = FileIconSize / 2 + Icon3_X;
	static constexpr int CircleCenterY = FileIconSize / 2;
	static constexpr int CircleColor = 0xFF0000;	// red

public:
	//*********************************************************************
	// Public interface
	//*********************************************************************
	void Open()
	{
		// Create graphics

		// FileIcons is an array of 4 32x32 icons with file info, as follows:
		// 0 - file
		// 1 - folder
		// 2 - to parent folder ("..:)
		// 3 - error

		// Set it as the canvas to draw on
		FillRect(&FileIcons, &FileIcons_Areas.All, ToolLibBackground);
		WriteSequentialRegisters(&FileIcons, CVSSA0, ImageRegCount);
		WriteSequentialRegisters(&s_fileIconsData, AWUL_X0, sizeof s_fileIconsData);

		// index 0: white rectangle for file
		FillRect(&FileIcons, &s_areaSheetIcon, SheetColor);

		// index 1: yellow rectangle for folder
		FillRect(&FileIcons, &s_areaFolderIcon, FolderColor);

		// index 2: left arrow for back to parent
		WriteSequentialRegisters(&s_arrowTrianglePoints, DLHSR0, sizeof s_arrowTrianglePoints);
		SetForeColor(ArrowColor);
		WriteReg(DCR0, DCR0_DrawTriangle | DCR0_FillOn | DCR0_DrawActive);
		WaitWhileBusy();
		FillRect(&FileIcons, &s_areaArrowBody, ArrowColor);

		// index 3: red circle for error
		WriteSequentialRegisters(&s_circleDesc, ELL_A0, sizeof s_circleDesc);
		SetForeColor(CircleColor);
		WriteReg(DCR1, DCR1_DrawEllipse | DCR1_FillOn | DCR1_DrawActive);
		WaitWhileBusy();

		// Create banner on main screen
		FillRect(&MainScreen, MainScreen.GetViewArea(), 0);
		FillRect(&MainScreen, &s_areaBanner, BannerBackColor);
		RectBorder(&MainScreen, &s_areaBanner, BannerBorderColor);
		s_bannerText.printf(BANNER_MSG);
		FillRect(&ScrollThumb, &ScrollThumb_Areas.Thumb, ScrollThumbColor);
		FillRect(&FileRow, FileRow.GetViewArea(), ToolLibBackground);
		FillRect(&UpdateDialog, UpdateDialog.GetViewArea(), UpdateBackground);

		DisplayVersions(
			FIRMWARE_LABEL, PROGRAM_VERSION,
			GRAPHICS_LABEL, GraphicsVersion,
			FONTS_LABEL, FontVersion);
		s_version.SetArea(UpdateDialog_Areas.ProgressBar);	// ready for reading headers

		FileOp.SetErrorHandler(FileError);
		UpdateDialog.SetHotspotList(&s_hotSpots);
		Lcd.EnablePip1(&UpdateDialog, 0, 0);
		FileBrowser::Open(&s_bufLine, ListUpdate, UpdateFileListHeight);
	}

	static void LoadGraphicsAction(uint spot, int x, int y)
	{
		switch (spot)
		{
			case UpdateExecute:
				UpdateMgr::SetLoadMode(UpdateMgr::EDIT_Update);
				FileOp.ReadUpdateHeader(FileBrowser::GetPathBuf());
				break;
		}
	}
	
	static void ListUpdate(FileBrowser::NotifyReason reason)
	{
		switch (reason)
		{
		case FileBrowser::REASON_FileSelected:
			// Callback when file/folder selected from list
			// Read and display version info
			UpdateMgr::SetLoadMode(UpdateMgr::EDIT_None);
			FileOp.ReadUpdateHeader(FileBrowser::GetPathBuf());
			break;

		case FileBrowser::REASON_DriveStatusChanged:
			if (IsMounted())
			{
				FileBrowser::FileError();	// clear errors
				ShowPip1();
				ShowPip2();
			}
			else
			{
				s_bufLine.DeleteText();
				s_version.ClearArea();
				HidePip1();
				HidePip2();
			}
			break;

		case FileBrowser::REASON_HeaderLoaded:
			DisplayVersions(
				UPDATE_LABEL, UpdateMgr::GetFirmwareVersion(), 
				"", UpdateMgr::GetGraphicsVersion(), 
				"", UpdateMgr::GetFontsVersion());
			ShowProgButton();
			s_isValidProg = true;
			ShowProgButton();
			return;

		case FileBrowser::REASON_InvalidHeader:
			s_version.printf(INVALID_HEADER);
			break;

		default:
			break;
		}

		s_isValidProg = false;
		ShowProgButton();
	}

	static int FileError(int err)
	{
		DEBUG_PRINT("File error %i\n", -err);
		FileBrowser::FileError(err);
		return err;
	}

	//*********************************************************************
	// Helpers
protected:
	static void DisplayVersions(
		const char *firmwareLabel, uint firmwareVersion, 
		const char *graphicsLabel, uint graphicsVersion, 
		const char *fontsLabel, uint fontsVersion)
	{
		s_version.printf("%-18s%-6u%-10s%-6u%-7s%-6u", firmwareLabel, firmwareVersion, graphicsLabel, graphicsVersion, fontsLabel, fontsVersion);
	}

	static void ShowProgButton()
	{
		FillRect(&UpdateDialog, &s_areaProgBtn, s_isValidProg ? ProgBtnColor : UpdateBackground);

		if (s_isValidProg)
		{
			s_progText.printf("Program\n");
		}
	}

	//*********************************************************************
	// Implement functions in ListScroll

protected:
	virtual void FillLine(int lineNum, Area *pArea)
	{
		FileBrowser::FillLine(lineNum, pArea, s_fixedFileRow);
	}

	virtual void LineSelected(int line)
	{
		s_version.ClearArea();
		FileBrowser::LineSelected(line);
	}

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const Area s_areaProgBtn =		{ ProgBtnLeft, ProgBtnTop, ProgBtnWidth, ProgBtnHeight };
	inline static const Area s_areaProgBtnText =	{ ProgBtnTextLeft, ProgBtnTextTop, ProgBtnTextWidth, ProgBtnTextHeight };
	inline static const Area s_areaBanner =			{ BannerLeft, BannerTop, BannerWidth, BannerHeight };
	inline static const Area s_areaBannerText =		{ BannerTextLeft, BannerTextTop, BannerWidth, BannerHeight };
	inline static const Area s_areaSheetIcon =		{ SheetX, SheetY, SheetWidth, SheetHeight };
	inline static const Area s_areaFolderIcon =		{ FolderX, FolderY, FolderWidth, FolderHeight };
	inline static const Area s_areaArrowBody =		{ ArrowBodyX, ArrowBodyY, ArrowBodyWidth, ArrowBodyHeight };

	// to use FileIcons as a canvas
	struct FileIconsData
	{
		ushort	viewInfo[4];
		byte	colorDepth;
	};
	inline static const FileIconsData s_fileIconsData = { {0, 0, 32 * 4, 32}, Color16bpp };
	
	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static bool s_isValidProg;

	inline static TextFieldFixed	s_fixedFileRow{FileRow, FileRow_Areas.FileName, 
		ToolLibForeground, ToolLibBackground, 
		CCR0_CharHeight24 | CCR0_CharSet8859_1, CCR1_CharBackgroundTransparent};

	inline static TextFieldFixed	s_version{UpdateDialog, UpdateDialog_Areas.CurrentVersion,
		UpdateForeground, UpdateBackground,
		CCR0_CharHeight24 | CCR0_CharSet8859_1, CCR1_CharBackgroundTransparent};

	inline static TextFieldFixed	s_progText{UpdateDialog, s_areaProgBtnText, ProgBtnTextColor, 
		ProgBtnColor, CCR0_CharHeight32 | CCR0_CharSet8859_1, CCR1_CharBackgroundSet};

	inline static TextFieldFixed	s_bannerText{MainScreen, s_areaBannerText, BannerTextColor, 
		BannerBackColor, CCR0_CharHeight32 | CCR0_CharSet8859_1, CCR1_CharHeightX2 | CCR1_CharWidthX2, true};

	inline static BufferedLine		s_bufLine{UpdateDialog, UpdateDialog_Areas.LoadFileName, FileBrowser::GetPathBuf(),
		FileBrowser::GetPathBufSize(), UpdateForeground, UpdateBackground,
		CCR0_CharHeight32 | CCR0_CharSet8859_1, CCR1_CharBackgroundTransparent};

	// Point lists for graphics
	inline static Location			s_arrowTrianglePoints[] = {
		{ArrowPointX, ArrowPointY}, {ArrowTopX, ArrowTopY}, {ArrowBottomX, ArrowBottomY}
	};

	inline static ushort s_circleDesc[] = { CircleRadius, CircleRadius, CircleCenterX, CircleCenterY };

	// Hotspot list, replacing the default for the UpdateDialog canvas
	struct LoadHotspotList
	{
		ushort	count;
		Hotspot	list[1];
	};
	inline static const LoadHotspotList s_hotSpots = { 1, {
		{ProgBtnLeft, ProgBtnTop, (ushort)(ProgBtnLeft + ProgBtnWidth - 1), 
			(ushort)(ProgBtnTop + ProgBtnHeight - 1), {UpdateExecute, HOTSPOT_GROUP_LoadGraphics}},
	}};
};
