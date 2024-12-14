//****************************************************************************
// FileBrowser.h
//
// Created 2/14/2021 12:38:50 PM by Tim
//
//****************************************************************************

#pragma once

#include "LcdDef.h"
#include "ListScroll.h"
#include "FileOperations.h"


class FileBrowser : public ListScroll
{
public:
	FileBrowser() : ListScroll(FileListWidth, FileListHeight, FileRowHeight, Color16bpp, HOTSPOT_GROUP_FileDisplay) {}

	//*********************************************************************
	// Public interface
	//*********************************************************************3
public:
	enum NotifyReason
	{
		REASON_FileSelected,
		REASON_FolderSelected,
		REASON_DriveStatusChanged,
		REASON_HeaderLoaded,
		REASON_InvalidHeader,
	};

	typedef void UpdateNotify(NotifyReason reason);

public:

public:
	static char *GetPathBuf()		{ return s_bufPath; }
	static ushort GetPathBufSize()	{ return sizeof s_bufPath; }
	static bool IsMounted()			{ return s_curBrowser->m_isMounted; }

public:
	void Open(BufferedLine *pBufLine, UpdateNotify *pfnNotify, uint height = FileListHeight)
	{
		Init();		// initialize ListScroll
		s_curBrowser = this;
		m_pBufLine = pBufLine;
		m_pfnNotify = pfnNotify;
		SetViewHeight(height);
		Lcd.EnablePip2(this, 0, ScreenHeight - height);
		FatSys::SetStatusNotify(FatDriveStatusChange);
		m_isMounted = FatSys::IsDriveMounted(0);
		DriveStatusChange(m_isMounted ? FatDrive::FDS_Mounted : FatDrive::FDS_Dismounted);
		Refresh();
	}

	static void Close()				
	{ 
		s_curBrowser = NULL;
		Lcd.DisablePip2(); 
	}

	void Refresh(bool fCreate = false)
	{
		// Should be called if edit box has new folder
		FindLastFolder(strlen(s_bufPath));		// sets m_cchPath
		m_pBufLine->UpdateBuffer();
		if (m_isMounted)
			FileOp.FolderEnum(s_bufPath, 0, m_cchPath, fCreate);
	}

	//*********************************************************************
	// Notifications

	// Notification from Actions class
public:
	static ListScroll *ListCapture(int x, int y, ScrollAreas spot)
	{
		return s_curBrowser->ScrollCapture(x, y, spot);
	}

protected:
	ListScroll *ScrollCapture(int x, int y, ScrollAreas spot)
	{
		if (StartCapture(x, y - Lcd.GetPip2()->y, spot))
			return this;
		return NULL;
	}

	// Notification from FileOperations class
public:
	static void FolderEnumDone(int cntFiles)
	{
		if (s_curBrowser != NULL)
			s_curBrowser->EnumDone(cntFiles);
	}

protected:
	void EnumDone(int cntFiles)
	{
		SetTotalLines(cntFiles);
		qsort(((ushort *)FILE_BUF_END) - cntFiles, cntFiles, sizeof(ushort), (__compar_fn_t)CompareLinePtr);
		InvalidateAllLines();
	}

	// Notification from FileOperations class
public:
	static void DriveMountComplete(int drive)
	{
		if (s_curBrowser != NULL)
			s_curBrowser->MountComplete();
	}

protected:
	void MountComplete()
	{
		m_isMounted = true;
		Refresh();

		if (m_pfnNotify != NULL)
			m_pfnNotify(REASON_DriveStatusChanged);
	}

	// Notification from ToolLib class, from FileOperations class
public:
	static void FileError(int err)
	{
		FileError(s_arErrMsg[-(err + 1)]);
	}

	static void FileError(const char *psz = NULL)
	{
		if (s_curBrowser != NULL)
			s_curBrowser->DriveError(psz);
	}

protected:
	void DriveError(const char *psz)
	{
		m_pErrMsg = psz;
		SetTotalLines(psz == NULL ? 0 : 1);
		InvalidateAllLines();
	}

	// Notification from FatSys class
public:
	static void FatDriveStatusChange(int drive, int status)
	{
		if (s_curBrowser != NULL)
			s_curBrowser->DriveStatusChange(status);
	}

protected:
	void DriveStatusChange(int status)
	{
		// Ignore Mount notification because the driver in 
		// FileOperations hasn't finished yet.
		if (status == FatDrive::FDS_Dismounted)
		{
			m_isMounted = false;
			DriveError(s_noDrivesMsg);
			Refresh();

			if (m_pfnNotify != NULL)
				m_pfnNotify(REASON_DriveStatusChanged);
		}
	}

	// Notification from UpdateMgr class
public:
	static void UpdateStatusChange(FileBrowser::NotifyReason reason)
	{
		s_curBrowser->StatusChange(reason);
	}

protected:
	void StatusChange(FileBrowser::NotifyReason reason)
	{
		if (m_pfnNotify != NULL)
			m_pfnNotify(reason);
	}

	//*********************************************************************
	// Helpers
	//*********************************************************************
protected:
	void FindLastFolder(int pos)
	{
		char	ch;

		while (pos > 0)
		{
			ch = s_bufPath[pos - 1];
			if (ch == '/' || ch == '\\')
				break;
			pos--;
		}
		m_cchPath = pos;
	}

	void FillLine(int lineNum, Area *pArea, TextField &row)
	{
		ulong		size;
		int			index;
		uint		hours;
		const char	*pAmPm;
		Area		area;
		FileEnumInfo	*pInfo;

		if (lineNum < m_lineCnt)
		{
			// Clear all text
			row.SetArea(FileRow_Areas.AllText);
			row.ClearArea();

			// Add file/folder icon
			pInfo = PtrInfoFromLine(lineNum);
			Lcd.SelectImage(&FileRow, &FileRow_Areas.FileIcon, &FileIcons, m_pErrMsg ? INFO_Error : pInfo->Type);

			if (m_pErrMsg)
			{
				// Just use the AllText area set above
				row.SetTextColor(ToolLibSelected);
				row.printf(m_pErrMsg);
				row.SetTextColor(ToolLibForeground);
			}
			else
			{
				if (pInfo->Type != INFO_Parent)
				{
					// Write filename
					row.SetArea(FileRow_Areas.FileName);
					row.printf(pInfo->Name);
				}

				if (pInfo->Type == INFO_File)
				{
					// Date
					row.SetSpaceWidth(row.GetCharWidth('0'));
					row.SetArea(FileRow_Areas.FileDate);
					row.printf("%2u/%02u/%u", pInfo->DateTime.date.month, 
						pInfo->DateTime.date.day, pInfo->DateTime.date.year + FAT_YEAR_BASE);

					// Time
					row.SetArea(FileRow_Areas.FileTime);
					hours = pInfo->DateTime.time.hours;
					pAmPm = s_amPm[hours < 12 ? 0 : 1];
					if (hours > 12)
						hours -= 12;
					else if (hours == 0)
						hours = 12;

					row.printf("%2u:%02u", hours, pInfo->DateTime.time.minutes);
					row.SetSpaceWidth(0);
					row.printf(" %s", pAmPm);
					row.SetSpaceWidth(row.GetCharWidth('0'));

					// Size
					row.SetArea(FileRow_Areas.FileSize);
					size = pInfo->Size;
					index = 0;
					if (size >= 1000000)
					{
						size /= 1000000;
						index = 2;
					}
					if (size >= 1000)
					{
						size /= 1000;
						index ++;
					}
					row.printf("%3lu", size);
					row.SetSpaceWidth(0);
					row.printf(" %s", s_fmtSize[index]);
				}
			}

			// Copy into place
			Lcd.CopyRect(this, pArea, &FileRow);
		}
		else
		{
			Lcd.FillRect(this, pArea, ToolLibBackground); 
			if (lineNum == m_lineCnt && m_lineCnt != 0)
			{
				// Draw bottom line of last grid entry.
				area = *pArea;
				area.Height = 1;
				Lcd.CopyRect(this, &area, &FileRow);
			}
		}
	}

	//*********************************************************************
	// Implement functions in ListScroll

protected:
	virtual void FillLine(int lineNum, Area *pArea)
	{
		FillLine(lineNum, pArea, s_fileRow);
	}

	virtual void LineSelected(int lineNum)
	{
		uint	cch;
		NotifyReason	reason = REASON_FileSelected;
		FileEnumInfo	*pInfo;

		if (m_pErrMsg == NULL)
		{
			pInfo = PtrInfoFromLine(lineNum);
			cch = strlen(pInfo->Name);
			// Make sure it fits, including possible trailing '/' and null terminator
			if (cch + m_cchPath >= sizeof s_bufPath - 2)
				return;

			if (pInfo->Type == INFO_Parent)
			{
				// Peel back one level
				if (m_cchPath != 0)
					FindLastFolder(m_cchPath - 1);
				goto EndFolder;
			}
			else
			{
				// Append to path
				memcpy(&s_bufPath[m_cchPath], pInfo->Name, cch + 1);

				if (pInfo->Type == INFO_Folder)
				{
					m_cchPath += cch;
					s_bufPath[m_cchPath++] = '/';
EndFolder:
					s_bufPath[m_cchPath] = '\0';
					FileOp.FolderEnum(s_bufPath, 0, m_cchPath);
					reason = REASON_FolderSelected;
				}
			}

			m_pBufLine->UpdateBuffer();
		}

		if (m_pfnNotify != NULL)
			m_pfnNotify(reason);
	}

	//*********************************************************************
	// static functions

protected:
	static FileEnumInfo *PtrInfoFromLine(int lineNum)
	{
		return (FileEnumInfo *)ADDOFFSET(g_FileBuf, *(((ushort *)FILE_BUF_END) - 1 - lineNum));
	}

	static int CompareLinePtr(const ushort *pOff1, const ushort *pOff2)
	{
		return CompareLines(*pOff2, *pOff1);
	}

	static int CompareLines(uint off1, uint off2)
	{
		FileEnumInfo	*pInfo1, *pInfo2;

		pInfo1 = (FileEnumInfo *)ADDOFFSET(g_FileBuf, off1);
		pInfo2 = (FileEnumInfo *)ADDOFFSET(g_FileBuf, off2);

		if (pInfo1->Type == pInfo2->Type)
			return strcasecmp(pInfo1->Name, pInfo2->Name);

		if (pInfo1->Type == INFO_Parent)
			return -1;

		if (pInfo2->Type == INFO_Parent)
			return 1;

		if (pInfo1->Type == INFO_Folder)
			return -1;

		return 1;
	}

	//*********************************************************************
	// instance (RAM) data
	//*********************************************************************
protected:
	bool			m_isMounted;
	ushort			m_cchPath;
	BufferedLine	*m_pBufLine;
	const char		*m_pErrMsg;
	UpdateNotify	*m_pfnNotify;

	//*********************************************************************
	// static (RAM) data
	//*********************************************************************
protected:
	inline static FileBrowser	*s_curBrowser;
	inline static TextField	s_fileRow{FileRow, FileRow_Areas.FileName, 
		FONT_CalcSmall, ToolLibForeground, ToolLibBackground};

	inline static char			s_bufPath[MAX_PATH + 1];

	//*********************************************************************
	// const (flash) data
	//*********************************************************************
protected:
	inline static const char s_fmtSize[4][3] = {"B ", "KB", "MB", "GB"};
	inline static const char s_amPm[2][3] = {"am", "pm"};

	//*********************************************************************
	// File error messages

	inline static const char s_noDrivesMsg[] = "No flash drive.";

	inline static const char * const s_arErrMsg[] = {
		// From Storage.h
		"Drive busy",
		"Drive not present",
		"Drive not mounted",
		"Bad block on drive",
		"Drive is write protected",
		"",	// Bad command
		"Drive failed",
		"", // Invalid block
		"Drive timed out",
		"Drive not available",

		// From FatFileConst.h
		"Drive can't be mounted",
		"Invalid drive identifier",
		"Too many files open",
		"", // Invalid handle
		"File/folder not found",
		"", // Invalid argument
		"", // File exists
		"Root folder is full",
		"Drive is full",
		"Bad character in file name",
		"Drive not formatted correctly",
		"Internal error",
		"", // Dirty file not closed
	};
};

extern FileBrowser Files;
