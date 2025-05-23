//****************************************************************************
// FileOperations.cpp
//
// Created 2/5/2021 11:20:03 AM by Tim
//
//****************************************************************************


#include <standard.h>
#include "Dro.h"
#include "FileOperations.h"
#include "ToolLib.h"
#include "UpdateMgr.h"


// Global buffer for file operations
byte g_FileBuf[FileBufSectors][FAT_SECT_SIZE] ALIGNED_ATTR(uint32_t);


void FileOperations::Process()
{
	int		status;
	int		cb;

	if (m_state == ST_Idle)
		return;

	if (m_hFile != 0)
		status = GetStatus(m_hFile);
	else
		status = GetDriveStatus(m_drive);

	if (status == FATERR_Busy)
		return;

	if (IsError(status))
	{
		OpDone();
		m_pfnError(status);
	}
	else
	{
		switch (m_state)
		{
			//*************************************************************
			// WriteFileToFlash / WriteToFlash

			// This state used by WriteFileToFlash
			OP_STATE(flash, open)
				flash.cbTotal = GetSize(m_hFile);
				goto FlashStartReadErase;
			END_STATE

			// This state used by WriteToFlash
			OP_STATE(flash, seek)
FlashStartReadErase:
				flash.erased = Lcd.SerialMemEraseStart(flash.addr, SerialFlashBlockSize, SerialFlashPort);
				goto FlashStartRead;
			END_STATE

			OP_STATE(flash, retry)
				// We've just completed a seek back in the file
				// Erase of flash sector (4K) in progress
FlashStartRead:
				StartRead(m_hFile, g_FileBuf, FAT_SECT_SIZE);
				TO_STATE(flash, read);
			END_STATE

			OP_STATE(flash, read)
				// status is no. of bytes read
				flash.cbBuf = status;
				flash.oBuf = 0;
				TO_STATE(flash, write);
				goto FlashCheckReady;
			END_STATE

			OP_STATE(flash, write)
FlashCheckReady:
				if (Lcd.IsSerialMemBusy())	// might be waiting for erase
					EXIT_STATE;
FlashWriteReady:
				flash.cbFlashed = Lcd.SerialMemWriteStart(flash.addr, flash.cbBuf, &g_FileBuf[0][flash.oBuf], SerialFlashPort);
				TO_STATE(flash, verify);
			END_STATE

			OP_STATE(flash, verify)
				if (Lcd.IsSerialMemBusy())
					EXIT_STATE;

				// Read back flash and verify
				Lcd.SerialMemRead(flash.addr, flash.cbFlashed, g_FileBuf[1], SerialFlashPort);
				uint	bRead, bDif;
				uint	cntErr = 0;
				byte	*pRead = g_FileBuf[1];
				byte	*pWritten = &g_FileBuf[0][flash.oBuf];
				for (cb = flash.cbFlashed; cb > 0; cb--)
				{
					bRead = *pRead++;
					bDif = *pWritten++ ^ bRead;
					if (bDif != 0)
					{
						cntErr++;
						// if all error bits are 1, then they are still "erased" and we can just
						// try again. Otherwise, we need to back up and erase again.
						if ((bDif & bRead) != bDif)
						{
							DEBUG_PRINT("Serial flash error at %lX, retrying sector.\n", flash.addr);
							long newAddr = flash.addr & ~(SerialFlashSectorSize - 1);
							Lcd.SerialMemEraseStart(newAddr, SerialFlashSectorSize, SerialFlashPort);
							long dif = newAddr - flash.addr;	// negative
							flash.erased -= dif;
							flash.cbTotal -= dif;
							flash.addr = newAddr;
							StartSeek(m_hFile, dif - flash.cbBuf, FAT_SEEK_CUR);
							TO_STATE(flash, retry);
							EXIT_STATE;
						}
					}
				}

				if (cntErr != 0)
				{
					DEBUG_PRINT("Serial flash error at %lX -- retrying page.\n", flash.addr);
					goto FlashWriteReady;
				}
				cb = flash.cbFlashed;
				flash.cbTotal -= cb;
				flash.addr += cb;
				flash.oBuf += cb;
				flash.cbBuf -= cb;
				flash.erased -= cb;
				DisplayProgress(cb);

				if (flash.cbTotal <= 0)
				{
					// All done
					uint hFile = m_hFile;
					OpDone();
					UpdateMgr::FlashWriteComplete(hFile);
					EXIT_STATE;
				}

				if (flash.erased == 0)
					goto FlashStartReadErase;	// overlap read and erase

				if (flash.cbBuf == 0)
					goto FlashStartRead;

				goto FlashWriteReady;
			END_STATE

			//*************************************************************
			// Mount

			OP_STATE(mount, ready)
				DEBUG_PRINT("Complete\n");
				OpDone();
				FileBrowser::DriveMountComplete(m_drive);
			END_STATE

			//*************************************************************
			// ToolImport

			OP_STATE(import, open)
				StartRead(m_hFile, g_FileBuf, FAT_SECT_SIZE);
				import.cbLeft = 0;
				TO_STATE(import, readStart);
			END_STATE

			OP_STATE(import, readStart)
				cb = status;
				if (cb > 0)
				{
					cb = Tools.ImportStart(cb);
					if (cb < 0)
						goto ImportClose;
					import.cbLeft = cb;
					import.pBuf = (char *)ADDOFFSET(g_FileBuf, status - cb);
					import.pErase = NULL;
					TO_STATE(import, erase);
				}
				else
					goto ImportClose;
			END_STATE

			OP_STATE(import, erase)
				void	*pv;
				pv = Tools.ImportErase(import.pErase);
				if (pv != NULL)
					import.pErase = pv;
				else
				{
					cb = Tools.ImportTools(import.pBuf, import.cbLeft, 0);
					goto ImportNext;
				}
			END_STATE

			OP_STATE(import, read0)
				// status is no. of bytes read
				cb = status;
				if (cb > 0)
				{
					if (import.cbLeft != 0)
						cb = Tools.ImportTools((char *)&g_FileBuf[2] - import.cbLeft, import.cbLeft, cb);
					else
						cb = Tools.ImportTools((char *)&g_FileBuf[0], cb, 0);

ImportNext:
					if (cb < 0)
						goto ImportClose;

					import.cbLeft = cb;
					StartRead(m_hFile, g_FileBuf[1], FAT_SECT_SIZE);
					TO_STATE(import, read1);
				}
				else
				{
ImportClose:
					Close(m_hFile);
					Tools.ImportDone(cb);
					OP_DONE;
				}
			END_STATE

			OP_STATE(import, read1)
				// status is no. of bytes read
				cb = status;
				if (cb > 0)
				{
					cb = Tools.ImportTools((char *)&g_FileBuf[1] - import.cbLeft, import.cbLeft + cb, 0);
					if (cb < 0)
						goto ImportClose;

					import.cbLeft = cb;
					StartRead(m_hFile, g_FileBuf[0], FAT_SECT_SIZE);
					TO_STATE(import, read0);
				}
				else
					goto ImportClose;
			END_STATE

			//*************************************************************
			// ToolExport

			OP_STATE(Export, open)
				Export.pBuf = Tools.ExportStart();
				Export.iTool = 0;
				Export.iBuf = 0;
				TO_STATE(Export, write);
			END_STATE

			OP_STATE(Export, write)
				char	*pBuf, *pBufRes;
				int		cb;

				pBuf = Export.pBuf;
				if (Export.iBuf == FileBufSectors - 1)
				{
					// Wrapped into last buffer, go back to start
					cb = pBuf - (char *)g_FileBuf[Export.iBuf];
					// Copy data from last buffer up to first
					memcpy(g_FileBuf, pBuf, cb);
					pBuf = (char *)g_FileBuf[0] + cb;
					Export.iBuf = 0;
				}
				do
				{
					pBufRes = Tools.ExportTool(pBuf, Export.iTool++);
					if (pBufRes == NULL)
					{
						// Reached end, flush last buffer
						cb = pBuf - (char *)g_FileBuf[Export.iBuf];
						StartWrite(m_hFile, g_FileBuf[Export.iBuf], cb);
						TO_STATE(Export, flush);
						EXIT_STATE;
					}
					pBuf = pBufRes;
				} while (pBuf < (char *)g_FileBuf[Export.iBuf + 1]);

				Export.pBuf = pBuf;
				StartWrite(m_hFile, g_FileBuf[Export.iBuf], FAT_SECT_SIZE);
				Export.iBuf++;
			END_STATE

			OP_STATE(Export, flush)
				StartClose(m_hFile);
				TO_STATE(Export, close);
			END_STATE

			OP_STATE(Export, close)
				OpDone();
				Tools.ExportDone();	// start folder enumeration
			END_STATE

			//*************************************************************
			// Folder enumeration

			OP_STATE(folder, open)
				int		h;

				h = EnumBegin(m_hFile);
				// May have created new folder, so can't use Close()
				StartClose(m_hFile);
				m_hFile = h;
				folder.cnt = 0;
				folder.pInfo = (FileEnumInfo *)g_FileBuf;
				TO_STATE(folder, close);
			END_STATE

			OP_STATE(folder, close)
				m_drive = StartEnumNext(m_hFile, folder.pInfo->Name, MAX_PATH + 1);
				TO_STATE(folder, name);
			END_STATE

			OP_STATE(folder, name)
				if (status == 0)
				{
					// Zero-length name means we're done
EndEnum:
					Close(m_hFile);
					FileBrowser::FolderEnumDone(folder.cnt);
					OP_DONE;
				}
				folder.cbName = status;	// remember length
				folder.pInfo->Size = GetSize(m_drive);
				StartGetDate(m_drive);
				TO_STATE(folder, date);
			END_STATE

			OP_STATE(folder, date)
				ushort	offset;
				ushort	*pList;

				folder.pInfo->DateTime = GetFatDate(m_drive);

				// Get buffer position
				offset = (byte *)folder.pInfo - (byte *)g_FileBuf;
				pList = ((ushort *)FILE_BUF_END) - 1 - folder.cnt;

				if (IsFolder(m_drive))
				{
					if (folder.pInfo->Name[0] == '.')
					{
						if (folder.pInfo->Name[1] == '\0')
							goto NextFolder;	// skip folder '.'

						if (folder.pInfo->Name[1] == '.' && folder.pInfo->Name[2] == '\0')
							folder.pInfo->Type = INFO_Parent;
						else
							folder.pInfo->Type = INFO_Folder;
					}
					else
						folder.pInfo->Type = INFO_Folder;
				}
				else
					folder.pInfo->Type = INFO_File;

				// Add to list at end of buffer
				*pList-- = offset;
				folder.cnt++;
				// Get offset for next FileEnumInfo
				// Note the name length does not include null terminator.
				folder.pInfo = folder.pInfo->Next(folder.cbName + 1);
NextFolder:
				Close(m_drive);
				// How much room is left in buffer?
				cb = (byte *)pList - (byte *)folder.pInfo - sizeof(FileEnumInfo);
				if (cb < FAT_MIN_NAME_BUF)
				{
					DEBUG_PRINT("Quit enum with %i bytes free:\n", cb);
					goto EndEnum;	// out of room, we're done
				}
				// Start next
				m_drive = StartEnumNext(m_hFile, folder.pInfo->Name, cb);
				TO_STATE(folder, name);
			END_STATE

			//*************************************************************
			// Read update header

			OP_STATE(header, open)
				StartRead(m_hFile, UpdateMgr::GetUpdateBuffer(), FAT_SECT_SIZE);
				TO_STATE(header, read);
			END_STATE

			OP_STATE(header, read)
				// status is no. of bytes read
				uint hFile = m_hFile;	// released by OpDone()
				OpDone();
				// hFile left open, to be closed by HeaderAvailable()
				UpdateMgr::HeaderAvailable((UpdateHeader *)UpdateMgr::GetUpdateBuffer(), status, hFile);
			END_STATE

			//*************************************************************
			// Read firmware into video RAM

#if UPDATE_FROM_VIDEO_RAM
			OP_STATE(update, seek)
				TO_STATE(update, read);
ReadMore:
				StartRead(m_hFile, g_FileBuf, FAT_SECT_SIZE);
			END_STATE

			OP_STATE(update, read)
				// status is no. of bytes read
				if (status != 0)
				{
					Lcd.WriteRam(update.addr, status, g_FileBuf);
					update.cb -= status;
					update.addr += status;
					if (update.cb > 0)
						goto ReadMore;
				}
				// UNDONE: status == 0 means we didn't read expected data
				uint hFile = m_hFile;
				OpDone();
				UpdateMgr::ReadUpdateComplete(hFile);
			END_STATE
#endif

		}
	}

	// Executed only when operation completed (OP_DONE)
	OpDone();
}
