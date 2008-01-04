//============================================================
//
//	softwarepicker.c - MESS's software picker
//
//============================================================

#define WIN32_LEAN_AND_MEAN
#include <assert.h>
#include <string.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>
#include <winuser.h>
#include <tchar.h>
#include <ctype.h>

#include "driver.h"
#include "unzip.h"
#include "strconv.h"
#include "hashfile.h"
#include "picker.h"
#include "screenshot.h"
#include "bitmask.h"
#include "winui.h"
#include "resourcems.h"
#include "mui_opts.h"
#include "softwarepicker.h"
#include "mui_util.h"



struct FileInfo
{
	device_class devclass;

	// hash information
	char szHash[HASH_BUF_SIZE];
	BOOL bHashRealized;
	const struct hash_info *pHashInfo;

	LPCSTR pszZipEntryName;
	LPCSTR pszSubName;
	char szFilename[1];
};

struct DirectorySearchInfo
{
	struct DirectorySearchInfo *pNext;
	HANDLE hFind;
	WIN32_FIND_DATA fd;
	char szDirectory[1];
};

struct SoftwarePickerInfo
{
	WNDPROC pfnOldWndProc;
	struct FileInfo **ppIndex;
	int nIndexLength;
	int nHashesRealized;
	int nCurrentPosition;
	struct DirectorySearchInfo *pFirstSearchInfo;
	struct DirectorySearchInfo *pLastSearchInfo;
	const game_driver *pDriver;
	hash_file *pHashFile;
	void (*pfnErrorProc)(const char *message);
};



static const TCHAR s_szSoftwarePickerProp[] = TEXT("SWPICKER");



static LPCSTR NormalizePath(LPCSTR pszPath, LPSTR pszBuffer, size_t nBufferSize)
{
	BOOL bChanged = FALSE;
	LPSTR s;
	int i, j;

	if (pszPath[0] == '\\')
	{
		win_get_current_directory_utf8(nBufferSize, pszBuffer);
		pszBuffer[2] = '\0';
		bChanged = TRUE;
	}
	else if (!_istalpha(pszPath[0]) || (pszPath[1] != ':'))
	{
		win_get_current_directory_utf8(nBufferSize, pszBuffer);
		bChanged = TRUE;
	}

	if (bChanged)
	{
		s = (LPSTR) alloca(strlen(pszBuffer) + 1);
		strcpy(s, pszBuffer);
		snprintf(pszBuffer, nBufferSize, "%s\\%s", s, pszPath);
		pszPath = pszBuffer;

		// Remove double path separators
		i = 0;
		j = 0;
		while(pszBuffer[i])
		{
			while ((pszBuffer[i] == '\\') && (pszBuffer[i+1] == '\\'))
				i++;
			pszBuffer[j++] = pszBuffer[i++];
		}
		pszBuffer[j] = '\0';
	}
	return pszPath;
}



static struct SoftwarePickerInfo *GetSoftwarePickerInfo(HWND hwndPicker)
{
	HANDLE h;
	h = GetProp(hwndPicker, s_szSoftwarePickerProp);
	assert(h);
	return (struct SoftwarePickerInfo *) h;
}



LPCSTR SoftwarePicker_LookupFilename(HWND hwndPicker, int nIndex)
{
	struct SoftwarePickerInfo *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->nIndexLength))
		return NULL;
	return pPickerInfo->ppIndex[nIndex]->szFilename;
}



device_class SoftwarePicker_LookupDevice(HWND hwndPicker, int nIndex)
{
	struct SoftwarePickerInfo *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->nIndexLength))
	{
		device_class dummy;
		memset(&dummy, 0, sizeof(dummy));
		return dummy;
	}
	return pPickerInfo->ppIndex[nIndex]->devclass;
}



int SoftwarePicker_LookupIndex(HWND hwndPicker, LPCSTR pszFilename)
{
	struct SoftwarePickerInfo *pPickerInfo;
	int i;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	for (i = 0; i < pPickerInfo->nIndexLength; i++)
	{
		if (!mame_stricmp(pPickerInfo->ppIndex[i]->szFilename, pszFilename))
			return i;
	}
	return -1;
}



iodevice_t SoftwarePicker_GetImageType(HWND hwndPicker, int nIndex)
{
	struct SoftwarePickerInfo *pPickerInfo;
	const device_class *devclass;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nIndex < 0) || (nIndex >= pPickerInfo->nIndexLength))
		return -1;

	devclass = &pPickerInfo->ppIndex[nIndex]->devclass;
	if (!devclass->gamedrv)
		return -1;
	return (iodevice_t) (int) device_get_info_int(devclass, DEVINFO_INT_TYPE);
}



void SoftwarePicker_SetDriver(HWND hwndPicker, const game_driver *pDriver)
{
	struct SoftwarePickerInfo *pPickerInfo;
	int i;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	if (pPickerInfo->pDriver != pDriver)
	{
		if (pPickerInfo->pHashFile)
		{
			hashfile_close(pPickerInfo->pHashFile);
			pPickerInfo->pHashFile = NULL;

			for (i = 0; i < pPickerInfo->nIndexLength; i++)
			{
				pPickerInfo->ppIndex[i]->pHashInfo = NULL;
				pPickerInfo->ppIndex[i]->bHashRealized = FALSE;
			}
			pPickerInfo->nHashesRealized = 0;
		}


		pPickerInfo->pDriver = pDriver;

		if (pDriver)
		{
			while(pDriver && !pPickerInfo->pHashFile)
			{
				pPickerInfo->pHashFile = hashfile_open_options(MameUIGlobal(), pDriver->name, TRUE, pPickerInfo->pfnErrorProc);
				pDriver = mess_next_compatible_driver(pDriver);
			}
		}
	}
}



void SoftwarePicker_SetErrorProc(HWND hwndPicker, void (*pfnErrorProc)(const char *message))
{
	struct SoftwarePickerInfo *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	pPickerInfo->pfnErrorProc = pfnErrorProc;
}



static void ComputeFileHash(struct SoftwarePickerInfo *pPickerInfo,
	struct FileInfo *pFileInfo, const unsigned char *pBuffer, unsigned int nLength)
{
	unsigned int nFunctions;
	iodevice_t type;
	device_partialhash_handler partialhash;

	type = (iodevice_t) (int) device_get_info_int(&pFileInfo->devclass, DEVINFO_INT_TYPE);
	partialhash = (device_partialhash_handler) device_get_info_fct(&pFileInfo->devclass, DEVINFO_PTR_PARTIAL_HASH);

	nFunctions = hashfile_functions_used(pPickerInfo->pHashFile, type);

	if (partialhash)
	{
		partialhash(pFileInfo->szHash, pBuffer, nLength, nFunctions);
	}
	else
	{
		hash_compute(pFileInfo->szHash, pBuffer, nLength, nFunctions);
	}
}



static BOOL SoftwarePicker_CalculateHash(HWND hwndPicker, int nIndex)
{
	struct SoftwarePickerInfo *pPickerInfo;
	struct FileInfo *pFileInfo;
	LPSTR pszZipName;
	BOOL rc = FALSE;
	unsigned char *pBuffer;
	unsigned int nLength;
	HANDLE hFile, hFileMapping;
	LVFINDINFO lvfi;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	assert((nIndex >= 0) && (nIndex < pPickerInfo->nIndexLength));
	pFileInfo = pPickerInfo->ppIndex[nIndex];

	if (pFileInfo->pszZipEntryName)
	{
		// this is in a ZIP file
		zip_file *zip;
		zip_error ziperr;
		const zip_file_header *zipent;

		// open the ZIP file
		nLength = pFileInfo->pszZipEntryName - pFileInfo->szFilename;
		pszZipName = (LPSTR) alloca(nLength);
		memcpy(pszZipName, pFileInfo->szFilename, nLength);
		pszZipName[nLength - 1] = '\0';

		// get the entry name
		ziperr = zip_file_open(pszZipName, &zip);
		if (ziperr == ZIPERR_NONE)
		{
			zipent = zip_file_first_file(zip);
			while(!rc && zipent)
			{
				if (!mame_stricmp(zipent->filename, pFileInfo->pszZipEntryName))
				{
					pBuffer = malloc(zipent->uncompressed_length);
					if (pBuffer)
					{
						ziperr = zip_file_decompress(zip, pBuffer, zipent->uncompressed_length);
						if (ziperr == ZIPERR_NONE)
						{
							ComputeFileHash(pPickerInfo, pFileInfo, pBuffer, zipent->uncompressed_length);
							rc = TRUE;
						}
						free(pBuffer);
					}
				}
				zipent = zip_file_next_file(zip);
			}
			zip_file_close(zip);
		}
	}
	else
	{
		// plain open file; map it into memory and calculate the hash
		hFile = win_create_file_utf8(pFileInfo->szFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, 0, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			nLength = GetFileSize(hFile, NULL);
			hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
			if (hFileMapping)
			{
				pBuffer = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
				if (pBuffer)
				{
					ComputeFileHash(pPickerInfo, pFileInfo, pBuffer, nLength);
					UnmapViewOfFile(pBuffer);
					rc = TRUE;
				}
				CloseHandle(hFileMapping);
			}
			CloseHandle(hFile);
		}
	}

	if (rc)
	{
		memset(&lvfi, 0, sizeof(lvfi));
		lvfi.flags = LVFI_PARAM;
		lvfi.lParam = nIndex;
		nIndex = ListView_FindItem(hwndPicker, -1, &lvfi);
		if (nIndex > 0)
			ListView_RedrawItems(hwndPicker, nIndex, nIndex);
	}

	return rc;
}



static void SoftwarePicker_RealizeHash(HWND hwndPicker, int nIndex)
{
	struct SoftwarePickerInfo *pPickerInfo;
	struct FileInfo *pFileInfo;
	unsigned int nHashFunctionsUsed = 0;
	unsigned int nCalculatedHashes = 0;
	iodevice_t type;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	assert((nIndex >= 0) && (nIndex < pPickerInfo->nIndexLength));
	pFileInfo = pPickerInfo->ppIndex[nIndex];

	// Determine which hash functions we need to use for this file, and which hashes
	// have already been calculated
	if ((pPickerInfo->pHashFile != NULL) && (pFileInfo->devclass.get_info != NULL))
	{
		type = (iodevice_t) (int) device_get_info_int(&pFileInfo->devclass, DEVINFO_INT_TYPE);
		if (type < IO_COUNT)
	        nHashFunctionsUsed = hashfile_functions_used(pPickerInfo->pHashFile, type);
		nCalculatedHashes = hash_data_used_functions(pFileInfo->szHash);
	}

	// Did we fully compute all hashes?
	if ((nHashFunctionsUsed & ~nCalculatedHashes) == 0)
	{
		// We have calculated all hashs for this file; mark it as realized
		pPickerInfo->ppIndex[nIndex]->bHashRealized = TRUE;
		pPickerInfo->nHashesRealized++;

		if (pPickerInfo->pHashFile)
		{
			pPickerInfo->ppIndex[nIndex]->pHashInfo = hashfile_lookup(pPickerInfo->pHashFile,
				pPickerInfo->ppIndex[nIndex]->szHash);
		}
	}
}



static BOOL SoftwarePicker_AddFileEntry(HWND hwndPicker, LPCSTR pszFilename,
	UINT nZipEntryNameLength, UINT32 nCrc, BOOL bForce)
{
	struct SoftwarePickerInfo *pPickerInfo;
	struct FileInfo **ppNewIndex;
	struct FileInfo *pInfo;
	int nIndex, nSize, devindex;
	LPCSTR pszExtension = NULL, s;
	const struct IODevice *devices = NULL;
	device_class devclass = {0,};

	// first check to see if it is already here
	if (SoftwarePicker_LookupIndex(hwndPicker, pszFilename) >= 0)
		return TRUE;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	// look up the device
	if (strrchr(pszFilename, '.'))
		pszExtension = strrchr(pszFilename, '.');
	if (pszExtension && pPickerInfo->pDriver)
	{
		pszExtension++;

		devices = devices_allocate(pPickerInfo->pDriver);
		if (devices != NULL)
		{
			for (devindex = 0; devices[devindex].type < IO_COUNT; devindex++)
			{
				s = devices[devindex].file_extensions;
				if (s)
				{
					while(*s && mame_stricmp(pszExtension, s))
						s += strlen(s) + 1;
					if (*s)
					{
						devclass = devices[devindex].devclass;
						break;
					}
				}
			}
			devices_free(devices);
		}
	}

	// no device?  cop out unless bForce is on
	if (!devclass.gamedrv && !bForce)
		return TRUE;

	// create the FileInfo structure
	nSize = sizeof(struct FileInfo) + strlen(pszFilename);
	pInfo = (struct FileInfo *) malloc(nSize);
	if (!pInfo)
		goto error;
	memset(pInfo, 0, nSize);

	// copy the filename
	strcpy(pInfo->szFilename, pszFilename);

	// set up device and CRC, if specified
	pInfo->devclass = devclass;
	if (devclass.gamedrv && device_get_info_fct(&devclass, DEVINFO_PTR_PARTIAL_HASH))
		nCrc = 0;
	if (nCrc != 0)
		snprintf(pInfo->szHash, sizeof(pInfo->szHash) / sizeof(pInfo->szHash[0]), "c:%08x#", nCrc);

	// set up zip entry name length, if specified
	if (nZipEntryNameLength > 0)
		pInfo->pszZipEntryName = pInfo->szFilename + strlen(pInfo->szFilename) - nZipEntryNameLength;

	// calculate the subname
	pInfo->pszSubName = strrchr(pInfo->szFilename, '\\');
	if (pInfo->pszSubName)
		pInfo->pszSubName++;
	else
		pInfo->pszSubName = pInfo->szFilename;

	ppNewIndex = realloc(pPickerInfo->ppIndex, (pPickerInfo->nIndexLength + 1) * sizeof(*pPickerInfo->ppIndex));
	if (!ppNewIndex)
		goto error;

	nIndex = pPickerInfo->nIndexLength++;
	pPickerInfo->ppIndex = ppNewIndex;
	pPickerInfo->ppIndex[nIndex] = pInfo;

	// Realize the hash
	SoftwarePicker_RealizeHash(hwndPicker, nIndex);

	// Actually insert the item into the picker
	Picker_InsertItemSorted(hwndPicker, nIndex);
	return TRUE;

error:
	if (pInfo)
		free(pInfo);
	return FALSE;
}



static BOOL SoftwarePicker_AddZipEntFile(HWND hwndPicker, LPCSTR pszZipPath,
	BOOL bForce, zip_file *pZip, const zip_file_header *pZipEnt)
{
	LPSTR s;
	LPCSTR temp = pZipEnt->filename;
	int nLength;
	int nZipEntryNameLength;

	// special case; skip first two characters if they are './'
	if ((pZipEnt->filename[0] == '.') && (pZipEnt->filename[1] == '/'))
	{
		while(*(++temp) == '/')
			;
	}

	nZipEntryNameLength = strlen(pZipEnt->filename);
	nLength = strlen(pszZipPath) + 1 + nZipEntryNameLength + 1;
	s = (LPSTR) alloca(nLength);
	snprintf(s, nLength, "%s\\%s", pszZipPath, pZipEnt->filename);

	return SoftwarePicker_AddFileEntry(hwndPicker, s,
		nZipEntryNameLength, pZipEnt->crc, bForce);
}

static BOOL SoftwarePicker_InternalAddFile(HWND hwndPicker, LPCSTR pszFilename,
	BOOL bForce)
{
	LPCSTR s;
	BOOL rc = TRUE;
	zip_error ziperr;
	zip_file *pZip;
	const zip_file_header *pZipEnt;

	s = strrchr(pszFilename, '.');
	if (s && !mame_stricmp(s, ".zip"))
	{
		ziperr = zip_file_open(pszFilename, &pZip);
		if (ziperr  == ZIPERR_NONE)
		{
			pZipEnt = zip_file_first_file(pZip);
			while(rc && pZipEnt)
			{
				rc = SoftwarePicker_AddZipEntFile(hwndPicker, pszFilename,
					bForce, pZip, pZipEnt);
				pZipEnt = zip_file_next_file(pZip);
			}
			zip_file_close(pZip);
		}
	}
	else
	{
		rc = SoftwarePicker_AddFileEntry(hwndPicker, pszFilename, 0, 0, bForce);
	}
	return rc;
}



BOOL SoftwarePicker_AddFile(HWND hwndPicker, LPCSTR pszFilename)
{
	char szBuffer[MAX_PATH];

	Picker_ResetIdle(hwndPicker);
	pszFilename = NormalizePath(pszFilename, szBuffer, sizeof(szBuffer)
		/ sizeof(szBuffer[0]));

	return SoftwarePicker_InternalAddFile(hwndPicker, pszFilename, TRUE);
}



BOOL SoftwarePicker_AddDirectory(HWND hwndPicker, LPCSTR pszDirectory)
{
	struct SoftwarePickerInfo *pPickerInfo;
	struct DirectorySearchInfo *pSearchInfo;
	struct DirectorySearchInfo **ppLast;
	size_t nSearchInfoSize;
	char szBuffer[MAX_PATH];

	pszDirectory = NormalizePath(pszDirectory, szBuffer, sizeof(szBuffer)
		/ sizeof(szBuffer[0]));

	Picker_ResetIdle(hwndPicker);
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	nSearchInfoSize = sizeof(struct DirectorySearchInfo) + strlen(pszDirectory);
	pSearchInfo = malloc(nSearchInfoSize);
	if (!pSearchInfo)
		return FALSE;
	memset(pSearchInfo, 0, nSearchInfoSize);
	pSearchInfo->hFind = INVALID_HANDLE_VALUE;

	strcpy(pSearchInfo->szDirectory, pszDirectory);

	// insert into linked list
	if (pPickerInfo->pLastSearchInfo)
		ppLast = &pPickerInfo->pLastSearchInfo->pNext;
	else
		ppLast = &pPickerInfo->pFirstSearchInfo;
	*ppLast = pSearchInfo;
	pPickerInfo->pLastSearchInfo = pSearchInfo;
	return TRUE;
}



static void SoftwarePicker_FreeSearchInfo(struct DirectorySearchInfo *pSearchInfo)
{
	if (pSearchInfo->hFind != INVALID_HANDLE_VALUE)
		FindClose(pSearchInfo->hFind);
	free(pSearchInfo);
}



static void SoftwarePicker_InternalClear(struct SoftwarePickerInfo *pPickerInfo)
{
	struct DirectorySearchInfo *p;
	int i;

	for (i = 0; i < pPickerInfo->nIndexLength; i++)
		free(pPickerInfo->ppIndex[i]);

	while(pPickerInfo->pFirstSearchInfo)
	{
		p = pPickerInfo->pFirstSearchInfo->pNext;
		SoftwarePicker_FreeSearchInfo(pPickerInfo->pFirstSearchInfo);
		pPickerInfo->pFirstSearchInfo = p;
	}

	pPickerInfo->ppIndex = NULL;
	pPickerInfo->nIndexLength = 0;
	pPickerInfo->nCurrentPosition = 0;
	pPickerInfo->pLastSearchInfo = NULL;
}



void SoftwarePicker_Clear(HWND hwndPicker)
{
	struct SoftwarePickerInfo *pPickerInfo;
	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	SoftwarePicker_InternalClear(pPickerInfo);
	ListView_DeleteAllItems(hwndPicker);
}



static BOOL SoftwarePicker_AddEntry(HWND hwndPicker,
	struct DirectorySearchInfo *pSearchInfo)
{
	struct SoftwarePickerInfo *pPickerInfo;
	LPSTR pszFilename;
	BOOL rc;
	char* utf8_FileName;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	utf8_FileName = utf8_from_tstring(pSearchInfo->fd.cFileName);
	if( !utf8_FileName )
		return FALSE;

	if (!strcmp(utf8_FileName, ".") || !strcmp(utf8_FileName, "..")) {
		free(utf8_FileName);
		return TRUE;
	}

	pszFilename = alloca(strlen(pSearchInfo->szDirectory) + 1 + strlen(utf8_FileName) + 1);
	strcpy(pszFilename, pSearchInfo->szDirectory);
	strcat(pszFilename, "\\");
	strcat(pszFilename, utf8_FileName);

	if (pSearchInfo->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		rc = SoftwarePicker_AddDirectory(hwndPicker, pszFilename);
	else
		rc = SoftwarePicker_InternalAddFile(hwndPicker, pszFilename, FALSE);

		free(utf8_FileName);
	return rc;
}



BOOL SoftwarePicker_Idle(HWND hwndPicker)
{
	struct SoftwarePickerInfo *pPickerInfo;
	struct FileInfo *pFileInfo;
	static const char szWildcards[] = "\\*.*";
	struct DirectorySearchInfo *pSearchInfo;
	LPSTR pszFilter;
	BOOL bSuccess;
	int nCount;
	BOOL bDone = FALSE;
	iodevice_t type;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);

	pSearchInfo = pPickerInfo->pFirstSearchInfo;
	if (pSearchInfo)
	{
		// searching through directories
		if (pSearchInfo->hFind != INVALID_HANDLE_VALUE)
		{
			bSuccess = FindNextFile(pSearchInfo->hFind, &pSearchInfo->fd);
		}
		else
		{
			pszFilter = alloca(strlen(pSearchInfo->szDirectory) + strlen(szWildcards) + 1);
			strcpy(pszFilter, pSearchInfo->szDirectory);
			strcat(pszFilter, szWildcards);
			pSearchInfo->hFind = win_find_first_file_utf8(pszFilter, &pSearchInfo->fd);
			bSuccess = pSearchInfo->hFind != INVALID_HANDLE_VALUE;
		}

		if (bSuccess)
		{
			SoftwarePicker_AddEntry(hwndPicker, pSearchInfo);
		}
		else
		{
			pPickerInfo->pFirstSearchInfo = pSearchInfo->pNext;
			if (!pPickerInfo->pFirstSearchInfo)
				pPickerInfo->pLastSearchInfo = NULL;
			SoftwarePicker_FreeSearchInfo(pSearchInfo);
		}
	}
	else if (pPickerInfo->pHashFile && (pPickerInfo->nHashesRealized
		< pPickerInfo->nIndexLength))
	{
		// time to realize some hashes
		nCount = 100;

		while((nCount > 0) && pPickerInfo->ppIndex[pPickerInfo->nCurrentPosition]->bHashRealized)
		{
			pPickerInfo->nCurrentPosition++;
			pPickerInfo->nCurrentPosition %= pPickerInfo->nIndexLength;
			nCount--;
		}

		pFileInfo = pPickerInfo->ppIndex[pPickerInfo->nCurrentPosition];
		if (!pFileInfo->bHashRealized)
		{
			type = (iodevice_t) (int) device_get_info_int(&pFileInfo->devclass, DEVINFO_INT_TYPE);
			if (hashfile_functions_used(pPickerInfo->pHashFile, type))
			{
				// only calculate the hash if it is appropriate for this device
				if (!SoftwarePicker_CalculateHash(hwndPicker, pPickerInfo->nCurrentPosition))
					return FALSE;
			}
			SoftwarePicker_RealizeHash(hwndPicker, pPickerInfo->nCurrentPosition);

			// under normal circumstances this will be redundant, but in the unlikely
			// event of a failure, we do not want to keep running into a brick wall
			// by calculating this hash over and over
			if (!pPickerInfo->ppIndex[pPickerInfo->nCurrentPosition]->bHashRealized)
			{
				pPickerInfo->ppIndex[pPickerInfo->nCurrentPosition]->bHashRealized = TRUE;
				pPickerInfo->nHashesRealized++;
			}
		}
	}
	else
	{
		// we are done!
		bDone = TRUE;
	}

	return !bDone;
}



LPCTSTR SoftwarePicker_GetItemString(HWND hwndPicker, int nRow, int nColumn,
	TCHAR *pszBuffer, UINT nBufferLength)
{
	struct SoftwarePickerInfo *pPickerInfo;
	const struct FileInfo *pFileInfo;
	LPCTSTR s = NULL;
	const char *pszUtf8 = NULL;
	unsigned int nHashFunction = 0;
	char szBuffer[256];
	TCHAR* t_buf;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	if ((nRow < 0) || (nRow >= pPickerInfo->nIndexLength))
		return NULL;

	pFileInfo = pPickerInfo->ppIndex[nRow];

	switch(nColumn)
	{
		case MESS_COLUMN_IMAGES:
			t_buf = tstring_from_utf8(pFileInfo->pszSubName);
			if( !t_buf )
				return s;
			_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
			s = pszBuffer;
			free(t_buf);
			break;

		case MESS_COLUMN_GOODNAME:
		case MESS_COLUMN_MANUFACTURER:
		case MESS_COLUMN_YEAR:
		case MESS_COLUMN_PLAYABLE:
			if (pFileInfo->pHashInfo)
			{
				switch(nColumn)
				{
					case MESS_COLUMN_GOODNAME:
						pszUtf8 = pFileInfo->pHashInfo->longname;
						break;
					case MESS_COLUMN_MANUFACTURER:
						pszUtf8 = pFileInfo->pHashInfo->manufacturer;
						break;
					case MESS_COLUMN_YEAR:
						pszUtf8 = pFileInfo->pHashInfo->year;
						break;
					case MESS_COLUMN_PLAYABLE:
						pszUtf8 = pFileInfo->pHashInfo->playable;
						break;
				}
				if (pszUtf8)
				{
					t_buf = tstring_from_utf8(pszUtf8);
					if( !t_buf )
						return s;
					_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
					s = pszBuffer;
					free(t_buf);
				}
			}
			break;

		case MESS_COLUMN_CRC:
		case MESS_COLUMN_MD5:
		case MESS_COLUMN_SHA1:
		switch (nColumn)
			{
				case MESS_COLUMN_CRC:	nHashFunction = HASH_CRC;	break;
				case MESS_COLUMN_MD5:	nHashFunction = HASH_MD5;	break;
				case MESS_COLUMN_SHA1:	nHashFunction = HASH_SHA1;	break;
			}
			if (hash_data_extract_printable_checksum(pFileInfo->szHash, nHashFunction, szBuffer))
			{
				t_buf = tstring_from_utf8(szBuffer);
				if( !t_buf )
					return s;
				_sntprintf(pszBuffer, nBufferLength, TEXT("%s"), t_buf);
				s = pszBuffer;
				free(t_buf);
			}
			break;
	}
	return s;
}



static LRESULT CALLBACK SoftwarePicker_WndProc(HWND hwndPicker, UINT nMessage,
	WPARAM wParam, LPARAM lParam)
{
	struct SoftwarePickerInfo *pPickerInfo;
	LRESULT rc;

	pPickerInfo = GetSoftwarePickerInfo(hwndPicker);
	rc = CallWindowProc(pPickerInfo->pfnOldWndProc, hwndPicker, nMessage, wParam, lParam);

	if (nMessage == WM_DESTROY)
	{
		SoftwarePicker_InternalClear(pPickerInfo);
		SoftwarePicker_SetDriver(hwndPicker, NULL);
		free(pPickerInfo);
	}

	return rc;
}



BOOL SetupSoftwarePicker(HWND hwndPicker, const struct PickerOptions *pOptions)
{
	struct SoftwarePickerInfo *pPickerInfo = NULL;
	LONG_PTR l;

	if (!SetupPicker(hwndPicker, pOptions))
		goto error;

	pPickerInfo = malloc(sizeof(*pPickerInfo));
	if (!pPickerInfo)
		goto error;
	memset(pPickerInfo, 0, sizeof(*pPickerInfo));

	if (!SetProp(hwndPicker, s_szSoftwarePickerProp, (HANDLE) pPickerInfo))
		goto error;

	l = (LONG_PTR) SoftwarePicker_WndProc;
	l = SetWindowLongPtr(hwndPicker, GWLP_WNDPROC, l);
	pPickerInfo->pfnOldWndProc = (WNDPROC) l;
	return TRUE;

error:
	if (pPickerInfo)
		free(pPickerInfo);
	return FALSE;
}
