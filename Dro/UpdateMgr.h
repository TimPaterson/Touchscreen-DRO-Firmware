//****************************************************************************
// UpdateMgr.h
//
// Created 3/9/2021 11:56:29 AM by Tim
//
//****************************************************************************

#pragma once

#include <Nvm/Nvm.h>
#include "VersionUpdate.h"
#include "ProgressBar.h"
#include "PowerDown.h"


class UpdateMgr
{
	static constexpr ulong ProgressBarForecolor = 0x00FF00;
	static constexpr ulong ProgressBarBackcolor = 0xFFFFFF;

	static constexpr int FlashRowSize = FLASH_PAGE_SIZE * NVMCTRL_ROW_PAGES;

public:
	enum EditMode
	{
		EDIT_None,
		EDIT_File,
		EDIT_Update,
		EDIT_HeaderError,
		EDIT_FileError,
	};

private:
	enum UpdateState
	{
		UPDT_None,
		UPDT_Firmware,
		UPDT_Graphics,
		UPDT_Fonts,
	};

	enum SectionMap
	{
		// bit map of sections found
		SECMAP_None = 0,
		SECMAP_Firmware = 1,
		SECMAP_Graphics = 2,
		SECMAP_Fonts = 4,
		SECMAP_All = SECMAP_Firmware | SECMAP_Graphics | SECMAP_Fonts
	};
	
	//*********************************************************************
	// Public interface
	//*********************************************************************3
public:
	static byte *GetUpdateBuffer()			{ return s_updateBuffer; }
	static uint GetFirmwareVersion()		{ return s_vFirmware; }
	static uint GetGraphicsVersion()		{ return s_vGraphics; }
	static uint GetFontsVersion()			{ return s_vFonts; }
	static void SetLoadMode(EditMode mode)	{ s_editMode = mode; }

public:
	static void Open()
	{
		Lcd.EnablePip1(&UpdateDialog, 0, 0);
		DisplayVersions(&UpdateDialog_Areas.CurrrentFirmware, PROGRAM_VERSION, GRAPHICS_VERSION, FONT_VERSION);
		FileOp.SetErrorHandler(FileError);
		Files.Open(&s_editFile, ListUpdate, UpdateFileListHeight);
	}

	static void Close()
	{
		if (s_editMode == EDIT_File)
			EndEdit();

		FileOp.SetErrorHandler(NULL);
		Lcd.DisablePip1();
		Lcd.DisablePip2();
	}
	
	static void UpdateAction(uint spot, int x, int)
	{
		switch (spot)
		{
		case FileName:
			x -= UpdateDialog_Areas.FileName.Xpos;	// relative edit box
			if (s_editMode != EDIT_File)
				StartEdit(x);
			else
				s_editFile.SetPositionPx(x); // Already editing file
			break;

		case ClearFile:
			s_editFile.DeleteText();
			StartEdit(0);
			break;

		case UpdateCancel:
			if (s_editMode == EDIT_FileError)
				StartEdit(EditLine::EndLinePx);
			else if (s_editMode == EDIT_File)
				EndEdit();
			else
				Close();
			break;

		case VersionMatch:
			s_fWriteAll ^= true;
			ShowVersionMatch();
			break;			

		case UpdateExecute:
			if (s_editMode == EDIT_File)
				EndEdit();

			else if (s_editMode == EDIT_FileError)
			{
				ClearFileError();
				break;
			}

			if (CheckIfFolder())
				Files.Refresh();
			else
			{
				// Read and display version info
				FileOp.ReadUpdateHeader(FileBrowser::GetPathBuf());
			}
			break;
		}
	}

	static void FileKeyHit(void *pvUser, uint key)
	{
		EditLine::EditStatus	edit;

		edit = s_editFile.ProcessKey(key);
		if (edit == EditLine::EditDone)
		{
			EndEdit();
			if (CheckIfFolder())
				Files.Refresh();
		}
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
	static void HeaderAvailable(UpdateHeader *pUpdate, int cb, uint hFile)
	{
		UpdateSection	*pSection;
		UpdateSection	*pFirmwareSection;
		UpdateSection	*pGraphicsSection;
		UpdateSection	*pFontsSection;
		uint	versionMap;
		ulong	cbTotal;

		// Verify this is a valid update file
		if (cb == FAT_SECT_SIZE && pUpdate->signature == s_signature.signature && pUpdate->countOfSections >= 3)
		{
			pSection = (UpdateSection *)ADDOFFSET(pUpdate, pUpdate->sectionsStart);
			versionMap = SECMAP_None;
			for (uint i = 0; i < pUpdate->countOfSections; i++)
			{
				// Make sure it fit in buffer
				if (ADDOFFSET(pSection, pUpdate->sectionSize) >= ADDOFFSET(pUpdate, FAT_SECT_SIZE))
					break;

				switch (pSection->progId)
				{
				case DroFirmwareId:
					if (pSection->dataSize > FLASH_SIZE)
						goto InvalidHeader;
					pFirmwareSection = pSection;
					s_vFirmware = pSection->progVersion;
					versionMap |= SECMAP_Firmware;
					break;

				case DroGraphicsId:
					pGraphicsSection = pSection;
					s_vGraphics = pSection->progVersion;
					versionMap |= SECMAP_Graphics;
					break;

				case DroFontId:
					pFontsSection = pSection;
					s_vFonts = pSection->progVersion;
					versionMap |= SECMAP_Fonts;
					break;

				default:
					continue;
				}

				// Skip to next section, per size in header
				pSection = (UpdateSection *)ADDOFFSET(pSection, pUpdate->sectionSize);

				if (versionMap == SECMAP_All)
				{
					if (s_editMode == EDIT_Update)
					{
						// Perform update
						s_pFirmwareSection = pFirmwareSection;
						cbTotal = pFirmwareSection->dataSize;

						// See if we're leaving some out
						if (!s_fWriteAll && pGraphicsSection->progVersion == GraphicsVersion)
						{
							pGraphicsSection = NULL;
						}
						else
							cbTotal += pGraphicsSection->dataSize;

						if (!s_fWriteAll && pFontsSection->progVersion == FontVersion)
						{
							pFontsSection = NULL;
						}
						else
							cbTotal += pFontsSection->dataSize;

						s_pGraphicsSection = pGraphicsSection;
						s_pFontsSection = pFontsSection;
						s_progress.SetMax(cbTotal);
						s_progress.SetValue(0);
						FileOp.SetProgressBar(&s_progress);

#if UPDATE_FROM_VIDEO_RAM
						// Step 1: Read program into video RAM
						FileOp.ReadFirmware(RamUpdateStart, pFirmwareSection->dataSize, pFirmwareSection->dataStart, hFile);
#else
						// Step 1: Read program into serial flash
						s_updateState = UPDT_Firmware;
						FileOp.WriteToFlash(FlashUpdateStart, pFirmwareSection->dataSize, pFirmwareSection->dataStart, hFile);
#endif
					}
					else
					{
						FatSys::Close(hFile);
						FileBrowser::UpdateStatusChange(FileBrowser::REASON_HeaderLoaded);
					}
					return;
				}
			}
		}

InvalidHeader:
		// Invalid header
		FatSys::Close(hFile);
		FileBrowser::UpdateStatusChange(FileBrowser::REASON_InvalidHeader);
	};
#pragma GCC diagnostic pop

#if UPDATE_FROM_VIDEO_RAM
	static void ReadUpdateComplete(uint hFile)
	{
		s_updateState = UPDT_Graphics;
		if (s_pGraphicsSection != NULL)
			FileOp.WriteToFlash(FlashScreenStart, s_pGraphicsSection->dataSize, s_pGraphicsSection->dataStart, hFile);
		else
			FlashWriteComplete(hFile);	// pretend we finished the graphics
	}
#endif

	static void FlashWriteComplete(uint hFile)
	{
		switch (s_updateState)
		{
#if !UPDATE_FROM_VIDEO_RAM
		case UPDT_Firmware:
			s_updateState = UPDT_Graphics;
			if (s_pGraphicsSection != NULL)
			{
				FileOp.WriteToFlash(FlashScreenStart, s_pGraphicsSection->dataSize, s_pGraphicsSection->dataStart, hFile);
				break;
			}
			//
			// Fall into case of Graphics complete
			//			
#endif
		case UPDT_Graphics:
			if (s_pFontsSection != NULL)
			{
				s_updateState = UPDT_Fonts;
				FileOp.WriteToFlash(FlashFontStart, s_pFontsSection->dataSize, s_pFontsSection->dataStart, hFile);
				break;
			}
			//
			// Fall into case of Fonts complete
			//
		case UPDT_Fonts:
			FatSys::Close(hFile);
#if UPDATE_FROM_VIDEO_RAM
			PrepFirmwareUpdate(RamUpdateStart);
#else
			PrepFirmwareUpdate(FlashUpdateStart);
#endif
			PowerDown::Save();	// keep our current position
			UpdateFirmware(s_pFirmwareSection->dataSize);
			break;

		default:
			// Single area write
			FatSys::Close(hFile);
			printf("complete.\n");
			break;
		}
	}

	static void SetEditMode(EditMode mode)
	{
		if (s_editMode == mode)
			return;

		s_versionText.SetArea(UpdateDialog_Areas.ProgressBar);
		s_versionText.ClearArea();

		s_editMode = mode;

		Lcd.SelectImage(&UpdateDialog, &UpdateDialog_Areas.UpdateButton, 
			&InspectUpdate, s_editMode == EDIT_Update);
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	static void StartEdit(int pos)
	{
		if (s_editMode == EDIT_FileError)
			ClearFileError();
		SetEditMode(EDIT_File);
		s_editFile.StartEditPx(pos);
		KeyboardMgr::OpenKb(FileKeyHit);
	}

	static void EndEdit()
	{
		s_editFile.EndEdit();
		s_editMode = EDIT_None;
		KeyboardMgr::CloseKb();
	}

	static bool CheckIfFolder()
	{
		char	chEnd;
		bool	isFolder;
		int		cch;

		cch = s_editFile.CharCount();
		if (cch == 0)
			isFolder = true;
		else
		{
			chEnd = Files.GetPathBuf()[cch - 1];
			isFolder = chEnd == '/' || chEnd == '\\';
		}
		return isFolder;
	}

	static void ListUpdate(FileBrowser::NotifyReason reason)
	{
		switch (reason)
		{
		case FileBrowser::REASON_FileSelected:
		case FileBrowser::REASON_FolderSelected:
			// Callback when file/folder selected from list
			if (s_editMode == EDIT_FileError)
				ClearFileError();
			SetEditMode(EDIT_None);
			break;

		case FileBrowser::REASON_DriveStatusChanged:
			if (s_editMode == EDIT_File)
				EndEdit();

			if (Files.IsMounted())
				ClearFileError();
			break;

		case FileBrowser::REASON_HeaderLoaded:
			SetEditMode(EDIT_Update);
			Lcd.CopyRect(&UpdateDialog, &UpdateDialog_Areas.UpdateLabel, &UpdateLabel);
			DisplayVersions(&UpdateDialog_Areas.UpdateFirmware, s_vFirmware, s_vGraphics, s_vFonts);
			break;

		case FileBrowser::REASON_InvalidHeader:
			SetEditMode(EDIT_HeaderError);
			s_versionText.SetArea(UpdateDialog_Areas.ProgressBar);
			s_versionText.WriteString(s_InvalidUpdateMsg);
			break;

		default:
			break;
		}
	}

	static void DisplayVersions(const Area *pAreas, uint firmwareVersion, uint graphicsVersion, uint fontsVersion) NO_INLINE_ATTR
	{
		s_versionText.SetArea(*pAreas++);
		s_versionText.printf("%u", firmwareVersion);
		s_versionText.SetArea(*pAreas++);
		s_versionText.printf("%u", graphicsVersion);
		s_versionText.SetArea(*pAreas);
		s_versionText.printf("%u", fontsVersion);
	}

	static void ShowVersionMatch()
	{
		Lcd.SelectImage(&UpdateDialog, &UpdateDialog_Areas.VersionMatch, &UpdateCheck, s_fWriteAll);
	}

	static void ClearFileError()
	{
		SetEditMode(EDIT_None);
		FileBrowser::FileError();
	}

	static int FileError(int err)
	{
		DEBUG_PRINT("File error %i\n", -err);

		// Inform file browser
		SetEditMode(EDIT_FileError);
		Files.FileError(err);

		return err;
	}

	static void PrepFirmwareUpdate(ulong addr)
	{
		WDT->CTRL.reg = 0;	// turn off WDT

#if UPDATE_FROM_VIDEO_RAM
		// Queue up RA8876 data port
		Lcd.WriteReg(AW_COLOR, AW_COLOR_AddrModeLinear | DATA_BUS_WIDTH);
		Lcd.WriteReg32(CURH0, addr);
		Lcd.ReadReg(MRWDP);	// dummy read
#else
		Lcd.SerialMemReadPrep(addr, 1);
#endif
	}

	//*********************************************************************
	// Functions in RAM used to program flash
	//*********************************************************************
protected:

#if !UPDATE_FROM_VIDEO_RAM
	RAMFUNC_ATTR static uint SerialReadByte()
	{
		uint val = Lcd.SerialReadByteInline();
		
		// SPIDR was left as current register by SerialReadByteInline()
		Lcd.WriteDataInline(0);	// seed next read
			
		return val;
	}
#endif
	
	RAMFUNC_ATTR NO_INLINE_ATTR static void UpdateFirmware(int cb)
	{
		ushort	*pFlash;
		ushort	*pBuf = NULL;

		pFlash = NULL;	// start programming at address zero
		__disable_irq();
		
#if !UPDATE_FROM_VIDEO_RAM
		// Pump out the response to command bytes
		for (int i = 0; i < 5; i++)
			SerialReadByte();
#endif

		while (cb > 0)
		{
			// if address is multiple of row size, we need to erase the row
			if (((int)pFlash & (FlashRowSize - 1)) == 0)
			{
				Nvm::EraseRowReady(pFlash);

				// Load entire flash row into buffer while erasing.
				pBuf = (ushort *)&s_updateBuffer;
				for (int i = 0; i < FlashRowSize / 2; i++)
				{
#if UPDATE_FROM_VIDEO_RAM
					*pBuf++ = Lcd.ReadData16Inline();
#else
					uint val = SerialReadByte();
					*pBuf++ = (SerialReadByte() << 8) + val;
#endif
				}

WaitErase:
				Nvm::WaitReadyInline();

				// Verify the page is erased
				for (int i = 0; i < FlashRowSize / 2; i++)
				{
					if (pFlash[i] != 0xFFFF)
					{
						WriteByte('E');
						Show((uint)pFlash / FlashRowSize);
						Nvm::EraseRowReady(pFlash);
						goto WaitErase;
					}
				}
				pBuf = (ushort *)&s_updateBuffer;
			}

			// Copy the data 16 bits at a time
			for (int i = FLASH_PAGE_SIZE; i > 0 && cb > 0; i -= 2, cb -= 2)
				*pFlash++ = *pBuf++;

			// Write the page
			Nvm::WritePageReady();
			Nvm::WaitReadyInline();
		}
		NVIC_SystemReset();
	}

#if	defined(DEBUG)
	RAMFUNC_ATTR NO_INLINE_ATTR static void WriteByte(byte ch)
	{
		Console.WriteByteNoBuf(ch);
		//while (!SERCOM0->USART.INTFLAG.bit.DRE);
		//SERCOM0->USART.DATA.reg = ch;
	}

	RAMFUNC_ATTR NO_INLINE_ATTR static void WriteDigit(byte digit)
	{
		digit = (digit & 0xF) + '0';
		if (digit > '9')
			digit += 'A' - '9' - 1;
		WriteByte(digit);
	}

	RAMFUNC_ATTR NO_INLINE_ATTR static void Show(int block)
	{
		WriteDigit(block >> 12);
		WriteDigit(block >> 8);
		WriteDigit(block >> 4);
		WriteDigit(block);
		WriteByte('\r');
		WriteByte('\n');
	}
#else
static inline void Show(int block) {}
static inline void WriteByte(byte ch) {}
#endif

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const union
	{
		char		archSignature[UpdateSignatureLength];
		uint64_t	signature;
	} s_signature = { UPDATE_SIGNATURE };

	inline static const char s_InvalidUpdateMsg[] = "Not a valid update file.";

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static byte	s_editMode;
	inline static byte	s_updateState;
	inline static bool	s_fWriteAll;
	inline static uint	s_vFirmware;
	inline static uint	s_vGraphics;
	inline static uint	s_vFonts;
	inline static UpdateSection	*s_pFirmwareSection;
	inline static UpdateSection	*s_pGraphicsSection;
	inline static UpdateSection	*s_pFontsSection;

	inline static EditLine		s_editFile{UpdateDialog, UpdateDialog_Areas.FileName, FileBrowser::GetPathBuf(),
		FileBrowser::GetPathBufSize(), FONT_CalcSmall, UpdateForeground, UpdateBackground};

	inline static TextField		s_versionText{UpdateDialog, UpdateDialog_Areas.CurrrentFirmware,
		FONT_CalcSmall, UpdateForeground, UpdateBackground};

	inline static ProgressBar	s_progress{UpdateDialog, UpdateDialog_Areas.ProgressBar,
		ProgressBarForecolor, ProgressBarBackcolor};

	inline static byte	s_updateBuffer[FAT_SECT_SIZE] ALIGNED_ATTR(uint32_t);
};
