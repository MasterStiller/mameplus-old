/***************************************************************************

  M.A.M.E.32  -  Multiple Arcade Machine Emulator for Win32
  Win32 Portions Copyright (C) 1997-2003 Michael Soderstrom and Chris Kirmse

  This file is part of MAME32, and may only be used, modified and
  distributed under the terms of the MAME license, in "readme.txt".
  By continuing to use, modify or distribute this file you indicate
  that you have read the license and understand and accept it fully.

***************************************************************************/

/***************************************************************************

  TreeView.c

  TreeView support routines - MSH 11/19/1998

***************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>  /* for sprintf */
#include <stdlib.h> /* For malloc and free */
#include <ctype.h> /* For tolower */
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _MSC_VER
#include <direct.h>
#endif

#include <io.h>
#include "MAME32.h"	// include this first
#include "driver.h"
#include "M32Util.h"
#include "bitmask.h"
#include "screenshot.h"
#include "TreeView.h"
#include "resource.h"
#include "properties.h"
#include "options.h"
#include "help.h"
#include "dialogs.h"
#include "translate.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#if _MSC_VER > 1200
#define HAS_DUMMYUNIONNAME
#endif
#endif

#define MAX_EXTRA_FOLDERS 256

/***************************************************************************
    public structures
 ***************************************************************************/

#define ICON_MAX (sizeof(treeIconNames) / sizeof(treeIconNames[0]))

/* Name used for user-defined custom icons */
/* external *.ico file to look for. */

typedef struct
{
	int		nResourceID;
	LPCSTR	lpName;
} TREEICON;

static TREEICON treeIconNames[] =
{
	{ IDI_FOLDER_OPEN,         "foldopen" },
	{ IDI_FOLDER,              "folder" },
	{ IDI_FOLDER_AVAILABLE,    "foldavail" },
	{ IDI_FOLDER_MANUFACTURER, "foldmanu" },
	{ IDI_FOLDER_UNAVAILABLE,  "foldunav" },
	{ IDI_FOLDER_YEAR,         "foldyear" },
	{ IDI_FOLDER_SOURCE,       "foldsrc" },
	{ IDI_MANUFACTURER,        "manufact" },
	{ IDI_WORKING,             "working" },
	{ IDI_NONWORKING,          "nonwork" },
	{ IDI_YEAR,                "year" },
	{ IDI_SOUND,               "sound" },
	{ IDI_CPU,                 "cpu" },
	{ IDI_HARDDISK,            "harddisk" },
	{ IDI_SOURCE,              "source" }
};

/***************************************************************************
    private variables
 ***************************************************************************/

/* this has an entry for every folder eventually in the UI, including subfolders */
static TREEFOLDER **treeFolders = 0;
static UINT         numFolders  = 0;        /* Number of folder in the folder array */
static UINT         next_folder_id = MAX_FOLDERS;
static UINT         folderArrayLength = 0;  /* Size of the folder array */
static LPTREEFOLDER lpCurrentFolder = 0;    /* Currently selected folder */
static UINT         nCurrentFolder = 0;     /* Current folder ID */
static WNDPROC      g_lpTreeWndProc = 0;    /* for subclassing the TreeView */
static HIMAGELIST   hTreeSmall = 0;         /* TreeView Image list of icons */

/* this only has an entry for each TOP LEVEL extra folder */
static LPEXFOLDERDATA ExtraFolderData[MAX_EXTRA_FOLDERS];
static int            numExtraFolders = 0;
static int            numExtraIcons = 0;
static char           *ExtraFolderIcons[MAX_EXTRA_FOLDERS];

// built in folders and filters
static LPFOLDERDATA  g_lpFolderData;
static LPFILTER_ITEM g_lpFilterList;	

static UINT          g_source_folder = 0;

/***************************************************************************
    private function prototypes
 ***************************************************************************/

extern BOOL         InitFolders(void);
static BOOL         CreateTreeIcons(void);
static void         TreeCtrlOnPaint(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static const char * ParseManufacturer(const char *s, int *pParsedChars );
static const char * TrimManufacturer(const char *s);
static void         CreateAllChildFolders(void);
static BOOL         AddFolder(LPTREEFOLDER lpFolder);
static LPTREEFOLDER NewFolder(const char *lpTitle, UINT nCategoryID, BOOL bTranslate, 
                              UINT nFolderId, int nParent, UINT nIconId,
                              DWORD dwFlags);
static void         DeleteFolder(LPTREEFOLDER lpFolder);

static LRESULT CALLBACK TreeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static int InitExtraFolders(void);
static void FreeExtraFolders(void);
static BOOL RegistExtraFolder(const char *name, LPEXFOLDERDATA *fExData, int msgcat, int icon, int subicon);
static void SetExtraIcons(char *name, int *id);
static BOOL TryAddExtraFolderAndChildren(int parent_index);

static BOOL TrySaveExtraFolder(LPTREEFOLDER lpFolder);

/***************************************************************************
    public functions
 ***************************************************************************/

/**************************************************************************
 *      ci_strncmp - case insensitive character array compare
 *
 *      Returns zero if the first n characters of s1 and s2 are equal,
 *      ignoring case.
 *		stolen from datafile.c
 **************************************************************************/
static int ci_strncmp (const char *s1, const char *s2, int n)
{
	int c1, c2;

	while (n)
	{
		if ((c1 = tolower (*s1)) != (c2 = tolower (*s2)))
			return (c1 - c2);
		else if (!c1)
			break;
		--n;

		s1++;
		s2++;
	}
	return 0;
}



/* De-allocate all folder memory */
void FreeFolders(void)
{
	int i = 0;

	if (treeFolders != NULL)
	{
		if (numExtraFolders)
		{
			FreeExtraFolders();
			numFolders -= numExtraFolders;
		}

		for (i = numFolders - 1; i >= 0; i--)
		{
			DeleteFolder(treeFolders[i]);
			treeFolders[i] = NULL;
			numFolders--;
		}
		free(treeFolders);
		treeFolders = NULL;
	}
	numFolders = 0;
}

/* Reset folder filters */
void ResetFilters(void)
{
	int i = 0;

	if (treeFolders != 0)
	{
		for (i = 0; i < (int)numFolders; i++)
		{
			treeFolders[i]->m_dwFlags &= ~F_MASK;

			/* Save the filters to the ini file */
			SetFolderFlags(treeFolders[i]->m_lpOriginalTitle?
						treeFolders[i]->m_lpOriginalTitle:
						treeFolders[i]->m_lpTitle, 0);
		}
	}
}

void InitTree(LPFOLDERDATA lpFolderData, LPFILTER_ITEM lpFilterList)
{
	g_lpFolderData = lpFolderData;
	g_lpFilterList = lpFilterList;

	InitFolders();

	/* this will subclass the treeview (where WM_DRAWITEM gets sent for
	   the header control) */
	g_lpTreeWndProc = (WNDPROC)(LONG)(int)GetWindowLong(GetTreeView(), GWL_WNDPROC);
	SetWindowLong(GetTreeView(), GWL_WNDPROC, (LONG)TreeWndProc);
}

void DestroyTree(HWND hWnd)
{
	if ( hTreeSmall )
	{
		ImageList_Destroy( hTreeSmall );
		hTreeSmall = NULL;
	}
}

void SetCurrentFolder(LPTREEFOLDER lpFolder)
{
	lpCurrentFolder = (lpFolder == 0) ? treeFolders[0] : lpFolder;
	nCurrentFolder = (lpCurrentFolder) ? lpCurrentFolder->m_nFolderId : 0;
}

LPTREEFOLDER GetCurrentFolder(void)
{
	return lpCurrentFolder;
}

UINT GetCurrentFolderID(void)
{
	return nCurrentFolder;
}

/*
int GetNumFolders(void)
{
	return numFolders;
}
*/

LPTREEFOLDER GetFolder(UINT nFolder)
{
	return (nFolder < numFolders) ? treeFolders[nFolder] : NULL;
}

LPTREEFOLDER GetFolderByID(UINT nID)
{
	UINT i;

	for (i = 0; i < numFolders; i++)
	{
		if (treeFolders[i]->m_nFolderId == nID)
		{
			return treeFolders[i];
		}
	}

	return (LPTREEFOLDER)0;
}

BOOL IsSourceFolder(LPTREEFOLDER lpFolder)
{
	return lpFolder->m_nParent == g_source_folder;
}

BOOL IsVectorFolder(LPTREEFOLDER lpFolder)
{
	const char *name = lpFolder->m_lpTitle;

	if ( lpFolder->m_nParent != -1)
		return FALSE;

	if (lpFolder->m_lpOriginalTitle)
		name = lpFolder->m_lpOriginalTitle;

	return strcmp(name, "Vector") == 0;
}

LPTREEFOLDER GetSourceFolder(int driver_index)
{
	const char *source_name = GetDriverFilename(driver_index);
	UINT i;

	for (i = 0; i < numFolders; i++)
	{
		LPTREEFOLDER lpFolder = treeFolders[i];
		const char *name = lpFolder->m_lpTitle;

		if (lpFolder->m_nParent != g_source_folder)
			continue;

		if (lpFolder->m_lpOriginalTitle)
			name = lpFolder->m_lpOriginalTitle;

		if (strcmp(name, source_name) == 0)
			return lpFolder;
	}

	return NULL;
}

void AddGame(LPTREEFOLDER lpFolder, UINT nGame)
{
	SetBit(lpFolder->m_lpGameBits, nGame);
}

void RemoveGame(LPTREEFOLDER lpFolder, UINT nGame)
{
	ClearBit(lpFolder->m_lpGameBits, nGame);
}

int FindGame(LPTREEFOLDER lpFolder, int nGame)
{
	return FindBit(lpFolder->m_lpGameBits, nGame, TRUE);
}

// Called to re-associate games with folders
void ResetWhichGamesInFolders(void)
{
	UINT	i, jj, k;
	BOOL b;
	int nGames = GetNumGames();

	for (i = 0; i < numFolders; i++)
	{
		LPTREEFOLDER lpFolder = treeFolders[i];

		// setup the games in our built-in folders
		for (k = 0; g_lpFolderData[k].m_lpTitle; k++)
		{
			if (lpFolder->m_nFolderId == g_lpFolderData[k].m_nFolderId)
			{
				if (g_lpFolderData[k].m_pfnQuery || g_lpFolderData[k].m_bExpectedResult)
				{
					SetAllBits(lpFolder->m_lpGameBits, FALSE);
					for (jj = 0; jj < nGames; jj++)
					{
						// invoke the query function
						b = g_lpFolderData[k].m_pfnQuery ? g_lpFolderData[k].m_pfnQuery(jj) : TRUE;

						// if we expect FALSE, flip the result
						if (!g_lpFolderData[k].m_bExpectedResult)
							b = !b;

						// if we like what we hear, add the game
						if (b)
							AddGame(lpFolder, jj);
					}
				}
				break;
			}
		}
	}
}


/* Used to build the GameList */
BOOL GameFiltered(int nGame, DWORD dwMask)
{
	int i;
	LPTREEFOLDER lpFolder = GetCurrentFolder();
	LPTREEFOLDER lpParent = NULL;

	// Filter games--return TRUE if the game should be HIDDEN in this view
	if( GetFilterInherit() )
	{
		if( lpFolder )
		{
			lpParent = GetFolder( lpFolder->m_nParent );
			if( lpParent )
			{
				/* Check the Parent Filters and inherit them on child,
				 * The inherited filters don't display on the custom Filter Dialog for the Child folder
				 * No need to promote all games to parent folder, works as is */
				dwMask |= lpParent->m_dwFlags;
			}
		}
	}
	/*Filter Text is already global*/

	if (MyStrStrI(drivers[nGame]->description,GetFilterText()) == NULL &&
		MyStrStrI(drivers[nGame]->name,GetFilterText()) == NULL && 
		MyStrStrI(drivers[nGame]->source_file,GetFilterText()) == NULL && 
		MyStrStrI(drivers[nGame]->manufacturer,GetFilterText()) == NULL)
	{
			return TRUE;
	}

	// Are there filters set on this folder?
	if ((dwMask & F_MASK) == 0)
		return FALSE;

	// Filter out clones?
	if (dwMask & F_CLONES && DriverIsClone(nGame))
		return TRUE;

	for (i = 0; g_lpFilterList[i].m_dwFilterType; i++)
	{
		if (dwMask & g_lpFilterList[i].m_dwFilterType)
		{
			if (g_lpFilterList[i].m_pfnQuery(nGame) == g_lpFilterList[i].m_bExpectedResult)
				return TRUE;
		}
	}
	return FALSE;
}

LPFILTER_ITEM GetFilterList(void)
{
	return g_lpFilterList;
}

/***************************************************************************
	private functions
 ***************************************************************************/

void CreateSourceFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int start_folder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	g_source_folder = parent_index;

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		const char *s = GetDriverFilename(jj);

		if (s == NULL || s[0] == '\0')
			continue;

		// look for an extant source treefolder for this game
		// (likely to be the previous one, so start at the end)
		for (i=numFolders-1;i>=start_folder;i--)
		{
			if (strcmp(treeFolders[i]->m_lpTitle,s) == 0)
			{
				AddGame(treeFolders[i],jj);
				break;
			}
		}
		if (i == start_folder-1)
		{
			// nope, it's a source file we haven't seen before, make it.
			LPTREEFOLDER lpTemp;
			lpTemp = NewFolder(s, 0, FALSE, next_folder_id++, parent_index, IDI_SOURCE,
			                   GetFolderFlags(s));
			AddFolder(lpTemp);
			AddGame(lpTemp,jj);
		}
	}
}

void CreateManufacturerFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int start_folder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER lpTemp;

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		const char *manufacturer = drivers[jj]->manufacturer;
		int iChars = 0;
		while( manufacturer != NULL && manufacturer[0] != '\0' )
		{
			const char *s = ParseManufacturer(manufacturer, &iChars);
			manufacturer += iChars;
			//shift to next start char
			if( s != NULL && strlen(s) > 0 )
 			{
				const char *t = TrimManufacturer(s);
				for (i=numFolders-1;i>=start_folder;i--)
				{
					//RS Made it case insensitive
					if (ci_strncmp(treeFolders[i]->m_lpOriginalTitle,t,20) == 0 )
					{
						AddGame(treeFolders[i],jj);
						break;
					}
				}
				if (i == start_folder-1)
				{
					// nope, it's a manufacturer we haven't seen before, make it.
					lpTemp = NewFolder(t, strcmp(s, "<unknown>") ? UI_MSG_MANUFACTURE : 0, TRUE, next_folder_id++, parent_index, IDI_MANUFACTURER,
					                   GetFolderFlags(s));
					AddFolder(lpTemp);
					AddGame(lpTemp,jj);
				}
			}
		}
	}
}

/* Make a reasonable name out of the one found in the driver array */
static const char* ParseManufacturer(const char *s, int *pParsedChars )
{
	static char tmp[256];
    char* ptmp;
	char *t;
	*pParsedChars= 0;

	if ( *s == '?' || *s == '<' || s[3] == '?' )
    {
		(*pParsedChars) = strlen(s);
		return "<unknown>";
    }

    ptmp = tmp;
	/*if first char is a space, skip it*/
	if( *s == ' ' )
	{
		(*pParsedChars)++;
        ++s;
	}
	while( *s )
	{
        /* combinations where to end string */
		
		if ( 
            ( (*s == ' ') && ( s[1] == '(' || s[1] == '/' || s[1] == '+' ) ) ||
            ( *s == ']' ) ||
            ( *s == '/' ) ||
            ( *s == '?' )
            )
        {
		(*pParsedChars)++;
			if( s[1] == '/' || s[1] == '+' )
				(*pParsedChars)++;
			break;
        }
		if( s[0] == ' ' && s[1] == '?' )
		{
			(*pParsedChars) += 2;
			s+=2;
		}

        /* skip over opening braces*/

		if ( *s != '[' )
        {
			*ptmp++ = *s;
	    }
		(*pParsedChars)++;
		/*for "distributed by" and "supported by" handling*/
		if( ( (s[1] == ',') && (s[2] == ' ') && ( (s[3] == 's') || (s[3] == 'd') ) ) )
		{
			//*ptmp++ = *s;
			++s;
			break;
		}
	        ++s;
	}
	*ptmp = '\0';
	t = tmp;
	if( tmp[0] == '(' || tmp[strlen(tmp)-1] == ')' || tmp[0] == ',')
	{
		ptmp = strchr( tmp,'(' );
		if ( ptmp == NULL )
		{
			ptmp = strchr( tmp,',' );
			if( ptmp != NULL)
			{
				//parse the new "supported by" and "distributed by"
				ptmp++;

				if (ci_strncmp(ptmp, " supported by", 13) == 0)
				{
					ptmp += 13;
				}
				else if (ci_strncmp(ptmp, " distributed by", 15) == 0)
				{
					ptmp += 15;
				}
				else
				{
					return NULL;
				}
			}
			else
			{
				ptmp = tmp;
				if ( ptmp == NULL )
				{
					return NULL;
				}
			}
		}
		if( tmp[0] == '(' || tmp[0] == ',')
		{
			ptmp++;
		}
		if (ci_strncmp(ptmp, "licensed from ", 14) == 0)
		{
			ptmp += 14;
		}
		// for the licenced from case
		if (ci_strncmp(ptmp, "licenced from ", 14) == 0)
		{
			ptmp += 14;
		}
		
		while ( (*ptmp != ')' ) && (*ptmp != '/' ) && *ptmp != '\0')
		{
			if (*ptmp == ' ' && ci_strncmp(ptmp, " license", 8) == 0)
			{
				break;
			}
			if (*ptmp == ' ' && ci_strncmp(ptmp, " licence", 8) == 0)
			{
				break;
			}
			*t++ = *ptmp++;
		}
		
		*t = '\0';
	}

	*ptmp = '\0';
	return tmp;
}

/* Analyze Manufacturer Names for typical patterns, that don't distinguish between companies (e.g. Co., Ltd., Inc., etc. */
static const char* TrimManufacturer(const char *s)
{
	int i=0;
	char strTemp[256];
	static char strTemp2[256];
	int j=0;
	int k=0;
	int l = 0;
	memset(strTemp, '\0', 256 );
	memset(strTemp2, '\0', 256 );
	//start analyzing from the back, as these are usually suffixes
	for(i=strlen(s)-1; i>=0;i-- )
	{
		
		l = strlen(strTemp);
		for(k=l;k>=0; k--)
			strTemp[k+1] = strTemp[k];
		strTemp[0] = s[i];
		strTemp[++l] = '\0';
		switch (l)
		{
			case 2:
				if( ci_strncmp(strTemp, "co", 2) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 3:
				if( ci_strncmp(strTemp, "co.", 3) == 0 || ci_strncmp(strTemp, "ltd", 3) == 0 || ci_strncmp(strTemp, "inc", 3) == 0)
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 4:
				if( ci_strncmp(strTemp, "inc.", 4) == 0 || ci_strncmp(strTemp, "ltd.", 4) == 0 || ci_strncmp(strTemp, "corp", 4) == 0)
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 5:
				if( ci_strncmp(strTemp, "corp.", 5) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 6:
				if( ci_strncmp(strTemp, "co-ltd", 6) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 7:
				if( ci_strncmp(strTemp, "co. ltd", 7) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 9:
				if( ci_strncmp(strTemp, "co., ltd.", 9) == 0 || ci_strncmp(strTemp, "gmbh & co", 9) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 10:
				if( ci_strncmp(strTemp, "corp, ltd.", 9) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			case 11:
				if( ci_strncmp(strTemp, "corporation", 11) == 0 )
				{
					j=l;
					while( s[strlen(s)-j-1] == ' ' || s[strlen(s)-j-1] == ',' )
					{
						j++;
					}
					if( j!=l)
					{
						memset(strTemp2, '\0', 256 );
						strncpy(strTemp2, s, strlen(s)-j );	
					}
				}
				break;
			default:
				break;
		}
	}
	if( strlen(strTemp2) == 0 )
		return s;
	return strTemp2;
}



void CreateCPUFolders(int parent_index)
{
	int i;
	int nGames = GetNumGames();
	int nFolder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER map[CPU_COUNT];

	memset(map, 0, sizeof map);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for ( i= 0; i < CPU_COUNT; i++)
	{
		LPTREEFOLDER lpTemp;
		char name[256];
		int jj;

		strcpy(name, cputype_shortname(i));
		if (name[0] == '\0')
			continue;

		for (jj = 0; jj < i; jj++)
			if (!strcmp(name, cputype_shortname(jj)))
				break;

		if (i != jj)
		{
			map[i] = map[jj];
			continue;
		}

		lpTemp = NewFolder(name, 0, FALSE, next_folder_id++, parent_index, IDI_CPU,
		                   GetFolderFlags(name));
		AddFolder(lpTemp);
		map[i] = treeFolders[nFolder++];
	}

	for (i = 0; i < nGames; i++)
	{
		machine_config drv;
		int n;

		expand_machine_driver(drivers[i]->drv, &drv);

		for (n = 0; n < MAX_CPU; n++)
		{
			if (drv.cpu[n].cpu_type == CPU_DUMMY)
				break;

			// cpu type #'s are one-based
			AddGame(map[drv.cpu[n].cpu_type], i);
		}
	}
}

void CreateSoundFolders(int parent_index)
{
	int i;
	int nGames = GetNumGames();
	int nFolder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER map[SOUND_COUNT];

	memset(map, 0, sizeof map);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (i = 0; i < SOUND_COUNT; i++)
	{
		LPTREEFOLDER lpTemp;
		char name[256];
		int jj;

		if (i == SOUND_FILTER_VOLUME
		 || i == SOUND_FILTER_RC
		 || i == SOUND_FILTER_LOWPASS)
			continue;

		strcpy(name, sndtype_shortname(i));
		if (name[0] == '\0')
			continue;

		for (jj = 0; jj < i; jj++)
			if (!strcmp(name, sndtype_shortname(jj)))
				break;

		if (i != jj)
		{
			map[i] = map[jj];
			continue;
		}

		lpTemp = NewFolder(name, 0, FALSE, next_folder_id++, parent_index, IDI_SOUND,
		                   GetFolderFlags(name));
		AddFolder(lpTemp);
		map[i] = treeFolders[nFolder++];
	}

	for (i = 0; i < nGames; i++)
	{
		machine_config drv;
		int n;

		expand_machine_driver(drivers[i]->drv, &drv);

		for (n = 0; n < MAX_SOUND; n++)
		{
			if (drv.sound[n].sound_type == SOUND_DUMMY)
				break;

			if (drv.sound[n].sound_type != SOUND_FILTER_VOLUME
			 && drv.sound[n].sound_type != SOUND_FILTER_RC
			 && drv.sound[n].sound_type != SOUND_FILTER_LOWPASS)
			{
				// sound type #'s are one-based, though that doesn't affect us here
				AddGame(map[drv.sound[n].sound_type], i);
			}
		}
	}
}

void CreateOrientationFolders(int parent_index)
{
	int jj;
	int nGames = GetNumGames();
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	// create our two subfolders
	LPTREEFOLDER lpVert, lpHorz;

	lpVert = NewFolder("Vertical", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
	                   GetFolderFlags("Vertical"));
	AddFolder(lpVert);

	lpHorz = NewFolder("Horizontal", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
	                   GetFolderFlags("Horizontal"));
	AddFolder(lpHorz);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		if (drivers[jj]->flags & ORIENTATION_SWAP_XY)
		{
			AddGame(lpVert,jj);
		}
		else
		{
			AddGame(lpHorz,jj);
		}
	}
}

void CreateDeficiencyFolders(int parent_index)
{
	static UINT32 deficiency_flags[] =
	{
		GAME_UNEMULATED_PROTECTION,
		GAME_WRONG_COLORS,
		GAME_IMPERFECT_COLORS,
		GAME_IMPERFECT_GRAPHICS,
		GAME_NO_SOUND,
		GAME_IMPERFECT_SOUND,
		GAME_NO_COCKTAIL,
	};

#define NUM_FLAGS	(sizeof (deficiency_flags) / sizeof (deficiency_flags[0]))

	static const char *deficiency_names[NUM_FLAGS] =
	{
		"Unemulated Protection",
		"Wrong Colors",
		"Imperfect Colors",
		"Imperfect Graphics",
		"Missing Sound",
		"Imperfect Sound",
		"No Cocktail"
	};

	int i, j;
	int nGames = GetNumGames();
	int nFolder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER map[NUM_FLAGS];

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (i = 0; i < NUM_FLAGS; i++)
	{
		LPTREEFOLDER lpTemp;

		lpTemp = NewFolder(deficiency_names[i], 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
		                   GetFolderFlags(deficiency_names[i]));
		AddFolder(lpTemp);
		map[i] = treeFolders[nFolder++];
	}

	for (i = 0; i < nGames; i++)
	{
		UINT32 flag = drivers[i]->flags;

		for (j = 0; j < NUM_FLAGS; j++)
			if (flag & deficiency_flags[j])
				AddGame(map[j],i);
	}
}

void CreateDumpingFolders(int parent_index)
{
	int jj;
	BOOL bBadDump  = FALSE;
	BOOL bNoDump = FALSE;
	int nGames = GetNumGames();
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	const rom_entry *region, *rom;
	const char *name;
	const game_driver *gamedrv;

	// create our two subfolders
	LPTREEFOLDER lpBad, lpNo;
	lpBad = NewFolder("Bad Dump", 0, TRUE, next_folder_id, parent_index, IDI_FOLDER,
 					   GetFolderFlags("Bad Dump"));
	AddFolder(lpBad);
	lpNo = NewFolder("No Dump", 0, TRUE, next_folder_id, parent_index, IDI_FOLDER,
 					   GetFolderFlags("No Dump"));
	AddFolder(lpNo);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		gamedrv = drivers[jj];

		if (!gamedrv->rom) 
			continue;
		bBadDump = FALSE;
		bNoDump = FALSE;
		for (region = rom_first_region(gamedrv); region; region = rom_next_region(region))
		{
			for (rom = rom_first_file(region); rom; rom = rom_next_file(rom))
			{
				if (ROMREGION_ISROMDATA(region) || ROMREGION_ISDISKDATA(region) )
				{
					name = ROM_GETNAME(rom);
					if (hash_data_has_info(ROM_GETHASHDATA(rom), HASH_INFO_BAD_DUMP))				
						bBadDump = TRUE;
					if (hash_data_has_info(ROM_GETHASHDATA(rom), HASH_INFO_NO_DUMP))				
						bNoDump = TRUE;
				}
			}
		}
		if (bBadDump)
		{
			AddGame(lpBad,jj);
		}
		if (bNoDump)
		{
			AddGame(lpNo,jj);
		}
	}
}


void CreateYearFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int start_folder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		char s[100];
		strcpy(s,drivers[jj]->year);

		if (s[0] == '\0')
			continue;

		if (s[4] == '?')
			s[4] = '\0';
		
		// look for an extant year treefolder for this game
		// (likely to be the previous one, so start at the end)
		for (i=numFolders-1;i>=start_folder;i--)
		{
			if (strncmp(treeFolders[i]->m_lpTitle,s,4) == 0)
			{
				AddGame(treeFolders[i],jj);
				break;
			}
		}
		if (i == start_folder-1)
		{
			// nope, it's a year we haven't seen before, make it.
			LPTREEFOLDER lpTemp;

			lpTemp = NewFolder(s, 0, !strcmp(s,"<unknown>"), next_folder_id++, parent_index, IDI_YEAR,
			                   GetFolderFlags(s));
			AddFolder(lpTemp);
			AddGame(lpTemp,jj);
		}
	}
}

#ifdef MISC_FOLDER
void CreateResolutionFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int start_folder = numFolders;
	machine_config drv;
	char Resolution[20];
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	// create our two subfolders - Vector (V) & Vector (H)
	LPTREEFOLDER lpVectorV, lpVectorH;
	lpVectorV = NewFolder("Vector (V)", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
 					   GetFolderFlags("Vector (V)"));
	AddFolder(lpVectorV);
	lpVectorH = NewFolder("Vector (H)", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
 					   GetFolderFlags("Vector (H)"));
	AddFolder(lpVectorH);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		expand_machine_driver(drivers[jj]->drv, &drv);

		if (drv.video_attributes & VIDEO_TYPE_VECTOR)
		{
			if (drivers[jj]->flags & ORIENTATION_SWAP_XY)
			{
				AddGame(lpVectorV,jj);
			}
			else
			{
				AddGame(lpVectorH,jj);
			}
		}
		else
		if (drivers[jj]->flags & ORIENTATION_SWAP_XY)
		{
			sprintf(Resolution, "%dx%d (V)",
						drv.default_visible_area.max_y - drv.default_visible_area.min_y + 1,
						drv.default_visible_area.max_x - drv.default_visible_area.min_x + 1);
		}
		else
		{
			sprintf(Resolution, "%dx%d (H)",
						drv.default_visible_area.max_x - drv.default_visible_area.min_x + 1,
						drv.default_visible_area.max_y - drv.default_visible_area.min_y + 1);
		}

		for (i=numFolders-1;i>=start_folder;i--)
		{
			if (strcmp(treeFolders[i]->m_lpTitle, Resolution) == 0)
			{
				AddGame(treeFolders[i],jj);
				break;
			}
		}
		if (i == start_folder-1)
		{
			LPTREEFOLDER lpTemp;
			lpTemp = NewFolder(Resolution, 0, FALSE, next_folder_id++, parent_index, IDI_FOLDER,
							   GetFolderFlags(Resolution));
			AddFolder(lpTemp);
			AddGame(lpTemp,jj);
		}
	}
}

void CreateAspectFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int start_folder = numFolders;
	machine_config drv;
	char Aspect[20];
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		expand_machine_driver(drivers[jj]->drv, &drv);

		if (drivers[jj]->flags & ORIENTATION_SWAP_XY)
		{
			if (drv.aspect_y == 0 && drv.aspect_x == 0)
			{
				drv.aspect_y = 3;
				drv.aspect_x = 4;
			}
			sprintf(Aspect, "%d:%d", drv.aspect_y, drv.aspect_x);
		}
		else
		{
			if (drv.aspect_x == 0 && drv.aspect_y == 0)
			{
				drv.aspect_x = 4;
				drv.aspect_y = 3;
			}
			sprintf(Aspect, "%d:%d", drv.aspect_x, drv.aspect_y);
		}

		for (i=numFolders-1;i>=start_folder;i--)
		{
			if (strcmp(treeFolders[i]->m_lpTitle, Aspect) == 0)
			{
				AddGame(treeFolders[i],jj);
				break;
			}
		}
		if (i == start_folder-1)
		{
			LPTREEFOLDER lpTemp;
			lpTemp = NewFolder(Aspect, 0, FALSE, next_folder_id++, parent_index, IDI_FOLDER,
							   GetFolderFlags(Aspect));
			AddFolder(lpTemp);
			AddGame(lpTemp,jj);
		}
	}
}

void CreateFPSFolders(int parent_index)
{
	int i,jj;
	int nGames = GetNumGames();
	int nFolder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER map[50];
	float fps[50];
	int nFPS = 0;

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (i = 0; i < nGames; i++)
	{
		LPTREEFOLDER lpTemp;
		float f;
		machine_config drv;

		expand_machine_driver(drivers[i]->drv,&drv);
		f = drv.frames_per_second;

		for (jj = 0; jj < nFPS; jj++)
			if (fps[jj] == f)
				break;

		if (nFPS == jj)
		{
			char buf[50];

			sprintf(buf, "%f Hz", f);

			lpTemp = NewFolder(buf, 0, FALSE, next_folder_id++, parent_index, IDI_FOLDER,
			                   GetFolderFlags(buf));
			AddFolder(lpTemp);
			map[nFPS] = treeFolders[nFolder++];
			fps[nFPS++] = f;
		}

		AddGame(map[jj],i);
	}
}

void CreateSaveStateFolders(int parent_index)
{
	int jj;
	int nGames = GetNumGames();
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	// create our two subfolders
	LPTREEFOLDER lpSupported, lpUnsupported;
	lpSupported = NewFolder("Supported", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
 					   GetFolderFlags("Supported"));
	AddFolder(lpSupported);
	lpUnsupported = NewFolder("Unsupported", 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
 					   GetFolderFlags("Unsupported"));
	AddFolder(lpUnsupported);

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (jj = 0; jj < nGames; jj++)
	{
		if (drivers[jj]->flags & GAME_SUPPORTS_SAVE)
		{
			AddGame(lpSupported,jj);
		}
		else
		{
			AddGame(lpUnsupported,jj);
		}
	}
}

void CreateControlFolders(int parent_index)
{
	enum {
		FOLDER_PLAYER1, FOLDER_PLAYER2, FOLDER_PLAYER3, FOLDER_PLAYER4,
		FOLDER_PLAYER5, FOLDER_PLAYER6, FOLDER_PLAYER7, FOLDER_PLAYER8,
		FOLDER_BUTTON1, FOLDER_BUTTON2, FOLDER_BUTTON3, FOLDER_BUTTON4, FOLDER_BUTTON5,
		FOLDER_BUTTON6, FOLDER_BUTTON7, FOLDER_BUTTON8, FOLDER_BUTTON9, FOLDER_BUTTON10,
		FOLDER_JOY2WAY, FOLDER_JOY4WAY, FOLDER_JOY8WAY,
//		FOLDER_VJOY2WAY,
		FOLDER_DOUBLEJOY2WAY, FOLDER_DOUBLEJOY4WAY, FOLDER_DOUBLEJOY8WAY,
//		FOLDER_VDOUBLEJOY2WAY,
		FOLDER_ADSTICK, FOLDER_PADDLE, FOLDER_DIAL, FOLDER_TRACKBALL, FOLDER_LIGHTGUN, FOLDER_PEDAL,
		FOLDER_MAX
	};

	static const char *ctrl_names[FOLDER_MAX] = {
		"Player 1",
		"Players 2",
		"Players 3",
		"Players 4",
		"Players 5",
		"Players 6",
		"Players 7",
		"Players 8",
		"Button 1",
		"Buttons 2",
		"Buttons 3",
		"Buttons 4",
		"Buttons 5",
		"Buttons 6",
		"Buttons 7",
		"Buttons 8",
		"Buttons 9",
		"Buttons 10",
		"Joy 2-Way",
		"Joy 4-Way",
		"Joy 8-Way",
//		"Joy 2-Way (V)",
		"Double Joy 2-Way",
		"Double Joy 4-Way",
		"Double Joy 8-Way",
//		"Double Joy 2-Way (V)",
		"AD Stick",
		"Paddle",
		"Dial",
		"Trackball",
		"Lightgun",
		"Pedal"
	};

	int i;
	int nGames = GetNumGames();
	int nFolder = numFolders;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];
	LPTREEFOLDER map[FOLDER_MAX];

	// no games in top level folder
	SetAllBits(lpFolder->m_lpGameBits,FALSE);

	for (i = 0; i < FOLDER_MAX; i++)
	{
		LPTREEFOLDER lpTemp;

		lpTemp = NewFolder(ctrl_names[i], 0, TRUE, next_folder_id++, parent_index, IDI_FOLDER,
		                   GetFolderFlags(ctrl_names[i]));
		AddFolder(lpTemp);
		map[i] = treeFolders[nFolder++];
	}

	for (i = 0; i < nGames; i++)
	{
		const input_port_entry *input;
		int b = 0;
		int p = 0;
		int w = 0;

		if (!drivers[i]->construct_ipt)
			continue;

		begin_resource_tracking();

		input = input_port_allocate(drivers[i]->construct_ipt, NULL);

		while (input->type != IPT_END)
		{
			int n;

			if (p < input->player + 1)
				p = input->player + 1;

				n = input->type - IPT_BUTTON1 + 1;

			if (n >= 1 && n <= MAX_NORMAL_BUTTONS && n > b)
			{
				b = n;
				continue;
			}

			switch (input->type)
			{
			case IPT_JOYSTICK_LEFT:
			case IPT_JOYSTICK_RIGHT:

				if (!w)
					w = FOLDER_JOY2WAY;
				break;

			case IPT_JOYSTICK_UP:
			case IPT_JOYSTICK_DOWN:

					if (input->four_way)
						w = FOLDER_JOY4WAY;
					else
						w = FOLDER_JOY8WAY;
				break;

			case IPT_JOYSTICKRIGHT_LEFT:
			case IPT_JOYSTICKRIGHT_RIGHT:
			case IPT_JOYSTICKLEFT_LEFT:
			case IPT_JOYSTICKLEFT_RIGHT:

				if (!w)
					w = FOLDER_DOUBLEJOY2WAY;
				break;

			case IPT_JOYSTICKRIGHT_UP:
			case IPT_JOYSTICKRIGHT_DOWN:
			case IPT_JOYSTICKLEFT_UP:
			case IPT_JOYSTICKLEFT_DOWN:

					if (input->four_way)
						w = FOLDER_DOUBLEJOY4WAY;
					else
						w = FOLDER_DOUBLEJOY8WAY;
				break;

			case IPT_PADDLE:
				AddGame(map[FOLDER_PADDLE],i);
				break;

			case IPT_DIAL:
				AddGame(map[FOLDER_DIAL],i);
				break;

			case IPT_TRACKBALL_X:
			case IPT_TRACKBALL_Y:
				AddGame(map[FOLDER_TRACKBALL],i);
				break;

			case IPT_AD_STICK_X:
			case IPT_AD_STICK_Y:
				AddGame(map[FOLDER_ADSTICK],i);
				break;

			case IPT_LIGHTGUN_X:
			case IPT_LIGHTGUN_Y:
				AddGame(map[FOLDER_LIGHTGUN],i);
				break;
			case IPT_PEDAL:
				AddGame(map[FOLDER_PEDAL],i);
				break;
			}
		++input;
		}

		if (p)
			AddGame(map[FOLDER_PLAYER1 + p - 1],i);

		if (b)
			AddGame(map[FOLDER_BUTTON1 + b - 1],i);

		if (w)
			AddGame(map[w],i);

		end_resource_tracking();
	}
}
#endif /* MISC_FOLDER */

static int compare_folder(const void *vp1, const void *vp2)
{
	const LPTREEFOLDER p1 = *(const LPTREEFOLDER *)vp1;
	const LPTREEFOLDER p2 = *(const LPTREEFOLDER *)vp2;
	const char *s1 = p1->m_lpTitle;
	const char *s2 = p2->m_lpTitle;

	while (*s1 && *s2)
	{
		if (isdigit(*s1) && isdigit(*s2))
		{
			int i1 = 0;
			int i2 = 0;

			while (isdigit(*s1))
			{
				i1 *= 10;
				i1 += *s1++ - '0';
			}

			while (isdigit(*s2))
			{
				i2 *= 10;
				i2 += *s2++ - '0';
			}

			if (i1 == i2)
				continue;

			return i1 - i2;
		}
		else if (*s1 != *s2)
			break;

		s1++;
		s2++;
	}

	return *s1 - *s2;
}

// creates child folders of all the top level folders, including custom ones
void CreateAllChildFolders(void)
{
	int num_top_level_folders = numFolders;
	int i, j;

	for (i = 0; i < num_top_level_folders; i++)
	{
		LPTREEFOLDER lpFolder = treeFolders[i];
		LPFOLDERDATA lpFolderData = NULL;
		UINT start_folder = numFolders;

		for (j = 0; g_lpFolderData[j].m_lpTitle; j++)
		{
			if (g_lpFolderData[j].m_nFolderId == lpFolder->m_nFolderId)
			{
				lpFolderData = &g_lpFolderData[j];
				break;
			}
		}

		if (lpFolderData != NULL)
		{
			//dprintf("Found built-in-folder id %i %i",i,lpFolder->m_nFolderId);
			if (lpFolderData->m_pfnCreateFolders != NULL)
				lpFolderData->m_pfnCreateFolders(i);
		}
		else
		{
			if ((lpFolder->m_dwFlags & F_CUSTOM) == 0)
			{
				dprintf("Internal inconsistency with non-built-in folder, but not custom");
				continue;
			}

			//dprintf("Loading custom folder %i %i",i,lpFolder->m_nFolderId);

			// load the extra folder files, which also adds children
			if (TryAddExtraFolderAndChildren(i) == FALSE)
			{
				lpFolder->m_nFolderId = FOLDER_NONE;
			}
		}

		qsort(&treeFolders[start_folder], numFolders - start_folder, sizeof (treeFolders[0]), compare_folder);
	}
}

// adds these folders to the treeview
void ResetTreeViewFolders(void)
{
	HWND hTreeView = GetTreeView();
	int i;
	TVITEM tvi;
	TVINSERTSTRUCT	tvs;

	HTREEITEM shti; // for current child branches

	// currently "cached" parent
	HTREEITEM hti_parent = NULL;
	int index_parent = -1;			

	TreeView_DeleteAllItems(hTreeView);

	//dprintf("Adding folders to tree ui indices %i to %i",start_index,end_index);

	tvs.hInsertAfter = TVI_LAST;

	for (i=0;i<numFolders;i++)
	{
		LPTREEFOLDER lpFolder = treeFolders[i];

		if (lpFolder->m_nParent == -1)
		{
			if (lpFolder->m_nFolderId < MAX_FOLDERS)
			{
				// it's a built in folder, let's see if we should show it
				if (GetShowFolder(lpFolder->m_nFolderId) == FALSE)
				{
					continue;
				}
			}

			tvi.mask	= TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
			tvs.hParent = TVI_ROOT;
			tvi.pszText = _Unicode(lpFolder->m_lpTitle);
			tvi.lParam	= (LPARAM)lpFolder;
			tvi.iImage	= GetTreeViewIconIndex(lpFolder->m_nIconId);
			tvi.iSelectedImage = 0;

#if !defined(NONAMELESSUNION)
			tvs.item = tvi;
#else
			tvs.DUMMYUNIONNAME.item = tvi;
#endif

			// Add root branch
			hti_parent = TreeView_InsertItem(hTreeView, &tvs);

			continue;
		}

		// not a top level branch, so look for parent
		if (treeFolders[i]->m_nParent != index_parent)
		{

			hti_parent = TreeView_GetRoot(hTreeView);
			while (1)
			{
				if (hti_parent == NULL)
				{
					// couldn't find parent folder, so it's a built-in but
					// not shown folder
					break;
				}

				tvi.hItem = hti_parent;
				tvi.mask = TVIF_PARAM;
				TreeView_GetItem(hTreeView,&tvi);
				if (((LPTREEFOLDER)tvi.lParam) == treeFolders[treeFolders[i]->m_nParent])
					break;

				hti_parent = TreeView_GetNextSibling(hTreeView,hti_parent);
			}

			// if parent is not shown, then don't show the child either obviously!
			if (hti_parent == NULL)
				continue;

			index_parent = treeFolders[i]->m_nParent;
		}

		tvi.mask	= TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
		tvs.hParent = hti_parent;
		tvi.iImage	= GetTreeViewIconIndex(treeFolders[i]->m_nIconId);
		tvi.iSelectedImage = 0;
		tvi.pszText = _Unicode(treeFolders[i]->m_lpTitle);
		tvi.lParam	= (LPARAM)treeFolders[i];

#if !defined(NONAMELESSUNION)
		tvs.item = tvi;
#else
		tvs.DUMMYUNIONNAME.item = tvi;
#endif
		// Add it to this tree branch
		shti = TreeView_InsertItem(hTreeView, &tvs);

	}
}

void SelectTreeViewFolder(int folder_id)
{
	HWND hTreeView = GetTreeView();
	HTREEITEM hti;
	TVITEM tvi;

	memset(&tvi,0,sizeof(tvi));

	hti = TreeView_GetRoot(hTreeView);

	while (hti != NULL)
	{
		HTREEITEM hti_next;

		tvi.hItem = hti;
		tvi.mask = TVIF_PARAM;
		TreeView_GetItem(hTreeView,&tvi);

		if (((LPTREEFOLDER)tvi.lParam)->m_nFolderId == folder_id)
		{
			TreeView_SelectItem(hTreeView,tvi.hItem);
			SetCurrentFolder((LPTREEFOLDER)tvi.lParam);
			return;
		}
		
		hti_next = TreeView_GetChild(hTreeView,hti);
		if (hti_next == NULL)
		{
			hti_next = TreeView_GetNextSibling(hTreeView,hti);
			if (hti_next == NULL)
			{
				hti_next = TreeView_GetParent(hTreeView,hti);
				if (hti_next != NULL)
					hti_next = TreeView_GetNextSibling(hTreeView,hti_next);
			}
		}
		hti = hti_next;
	}

	// could not find folder to select
	// make sure we select something
	tvi.hItem = TreeView_GetRoot(hTreeView);
	tvi.mask = TVIF_PARAM;
	TreeView_GetItem(hTreeView,&tvi);

	TreeView_SelectItem(hTreeView,tvi.hItem);
	SetCurrentFolder((LPTREEFOLDER)tvi.lParam);

}

/* Add a folder to the list.  Does not allocate */
static BOOL AddFolder(LPTREEFOLDER lpFolder)
{
	if (numFolders + 1 >= folderArrayLength)
	{
		folderArrayLength += 500;
		treeFolders = realloc(treeFolders,sizeof(TREEFOLDER **) * folderArrayLength);
	}

	treeFolders[numFolders] = lpFolder;
	numFolders++;

	return TRUE;
}

/* Allocate and initialize a NEW TREEFOLDER */
static LPTREEFOLDER NewFolder(const char *lpTitle, UINT nCategoryID, BOOL bTranslate, 
					   UINT nFolderId, int nParent, UINT nIconId, DWORD dwFlags)
{
	LPTREEFOLDER lpFolder = (LPTREEFOLDER)malloc(sizeof(TREEFOLDER));

	memset(lpFolder, '\0', sizeof (TREEFOLDER));

	if (bTranslate)
	{
		if (!nCategoryID)
			nCategoryID = UI_MSG_UI;
		lpFolder->m_nCategoryID = nCategoryID;

		lpFolder->m_lpOriginalTitle = (LPSTR)malloc(strlen(lpTitle) + 1);
		strcpy((char *)lpFolder->m_lpOriginalTitle,lpTitle);
		lpTitle = lang_message(nCategoryID, lpTitle);
	}
	lpFolder->m_lpTitle = (LPSTR)malloc(strlen(lpTitle) + 1);
	strcpy((char *)lpFolder->m_lpTitle,lpTitle);
	lpFolder->m_lpGameBits = NewBits(GetNumGames());
	lpFolder->m_nFolderId = nFolderId;
	lpFolder->m_nParent = nParent;
	lpFolder->m_nIconId = nIconId;
	lpFolder->m_dwFlags = dwFlags;

	return lpFolder;
}

/* Deallocate the passed in LPTREEFOLDER */
static void DeleteFolder(LPTREEFOLDER lpFolder)
{
	if (lpFolder)
	{
		if (lpFolder->m_lpGameBits)
		{
			DeleteBits(lpFolder->m_lpGameBits);
			lpFolder->m_lpGameBits = 0;
		}
		free(lpFolder->m_lpTitle);
		lpFolder->m_lpTitle = 0;
		FreeIfAllocated(&lpFolder->m_lpOriginalTitle);
		free(lpFolder);
		lpFolder = 0;
	}
}

/* Can be called to re-initialize the array of treeFolders */
BOOL InitFolders(void)
{
	int             i = 0;
	DWORD           dwFolderFlags;
	LPFOLDERDATA    fData = 0;
	BOOL            doCreateFavorite;

	if (treeFolders != NULL)
	{
		for (i = numFolders - 1; i >= 0; i--)
		{
			DeleteFolder(treeFolders[i]);
			treeFolders[i] = 0;
			numFolders--;
		}
	}
	numFolders = 0;
	if (folderArrayLength == 0)
	{
		folderArrayLength = 200;
		treeFolders = (TREEFOLDER **)malloc(sizeof(TREEFOLDER **) * folderArrayLength);
		if (!treeFolders)
		{
			folderArrayLength = 0;
			return 0;
		}
		else
		{
			memset(treeFolders,'\0', sizeof(TREEFOLDER **) * folderArrayLength);
		}
	}
	// built-in top level folders
	for (i = 0; g_lpFolderData[i].m_lpTitle; i++)
	{
		fData = &g_lpFolderData[i];
		/* get the saved folder flags */
		dwFolderFlags = GetFolderFlags(fData->m_lpTitle);
		/* create the folder */
		AddFolder(NewFolder(fData->m_lpTitle, 0, TRUE, fData->m_nFolderId, -1,
		                    fData->m_nIconId, dwFolderFlags));
	}

	numExtraFolders = InitExtraFolders();
	doCreateFavorite = TRUE;

	for (i = 0; i < numExtraFolders; i++)
	{
		if (mame_stricmp(ExtraFolderData[i]->m_szTitle, extFavorite.title) == 0)
			doCreateFavorite = FALSE;
	}

	dprintf("I %shave %s", doCreateFavorite ? "don't " : "", extFavorite.title);
	if (doCreateFavorite)
	{
		int rooticon, subicon;
		char *filename;
		char *rootname = strdup(extFavorite.root_icon);
		char *subname = strdup(extFavorite.sub_icon);

		filename = malloc(strlen(extFavorite.title) + sizeof (".ini"));
		sprintf(filename, "%s.ini", extFavorite.title);

		SetExtraIcons(rootname, &rooticon);
		SetExtraIcons(subname, &subicon);

		if (RegistExtraFolder(filename, &ExtraFolderData[numExtraFolders], UI_MSG_EXTRA + numExtraFolders, rooticon, subicon))
			numExtraFolders++;
		else
			doCreateFavorite = FALSE;

		free(filename);
		free(rootname);
		free(subname);
	}

	for (i = 0; i < numExtraFolders; i++)
	{
		LPEXFOLDERDATA fExData = ExtraFolderData[i];
		LPTREEFOLDER   lpFolder;

		// OR in the saved folder flags
		dwFolderFlags = fExData->m_dwFlags | GetFolderFlags(fExData->m_szTitle);
		// create the folder
		//dprintf("creating top level custom folder with icon %i",fExData->m_nIconId);
		lpFolder = NewFolder(fExData->m_szTitle,UI_MSG_EXTRA + (fExData->m_nFolderId - MAX_FOLDERS),TRUE,fExData->m_nFolderId,fExData->m_nParent,
		                    fExData->m_nIconId,dwFolderFlags);
		AddFolder(lpFolder);

		if (doCreateFavorite && i == numExtraFolders - 1)
		{
			if (TrySaveExtraFolder(lpFolder))
				dprintf("created: %s.ini", fExData->m_szTitle);
		}
	}

	CreateAllChildFolders();

	CreateTreeIcons();

	ResetWhichGamesInFolders();

	ResetTreeViewFolders();

	SelectTreeViewFolder(GetSavedFolderID());

	return TRUE;
}

// create iconlist and Treeview control
static BOOL CreateTreeIcons(void)
{
	HICON	hIcon;
	INT 	i;
	HINSTANCE hInst = GetModuleHandle(0);

	int numIcons = ICON_MAX + numExtraIcons;

	hTreeSmall = ImageList_Create (16, 16, ILC_COLORDDB | ILC_MASK, numIcons, numIcons);

	//dprintf("Trying to load %i normal icons",ICON_MAX);
	for (i = 0; i < ICON_MAX; i++)
	{
		hIcon = LoadIconFromFile(treeIconNames[i].lpName);
		if (!hIcon)
			hIcon = LoadIcon(hInst, MAKEINTRESOURCE(treeIconNames[i].nResourceID));

		if (ImageList_AddIcon (hTreeSmall, hIcon) == -1)
		{
			ErrorMsg("Error creating icon on regular folder, %i %i",i,hIcon != NULL);
			return FALSE;
		}
	}

	//dprintf("Trying to load %i extra custom-folder icons",numExtraIcons);
	for (i = 0; i < numExtraIcons; i++)
	{
		if ((hIcon = LoadIconFromFile(ExtraFolderIcons[i])) == 0)
			hIcon = LoadIcon (hInst, MAKEINTRESOURCE(IDI_FOLDER));

		if (ImageList_AddIcon(hTreeSmall, hIcon) == -1)
		{
			ErrorMsg("Error creating icon on extra folder, %i %i",i,hIcon != NULL);
			return FALSE;
		}
	}

	// Be sure that all the small icons were added.
	if (ImageList_GetImageCount(hTreeSmall) < numIcons)
	{
		ErrorMsg("Error with icon list--too few images.  %i %i",
				ImageList_GetImageCount(hTreeSmall),numIcons);
		return FALSE;
	}

	// Be sure that all the small icons were added.

	if (ImageList_GetImageCount (hTreeSmall) < ICON_MAX)
	{
		ErrorMsg("Error with icon list--too few images.  %i < %i",
				 ImageList_GetImageCount(hTreeSmall),ICON_MAX);
		return FALSE;
	}

	// Associate the image lists with the list view control.
	TreeView_SetImageList(GetTreeView(), hTreeSmall, TVSIL_NORMAL);

	return TRUE;
}


static void TreeCtrlOnPaint(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC 		hDC;
	RECT		rcClip, rcClient;
	HDC 		memDC;
	HBITMAP 	bitmap;
	HBITMAP 	hOldBitmap;

	HBITMAP hBackground = GetBackgroundBitmap();
	MYBITMAPINFO *bmDesc = GetBackgroundInfo();

	hDC = BeginPaint(hWnd, &ps);

	GetClipBox(hDC, &rcClip);
	GetClientRect(hWnd, &rcClient);

	// Create a compatible memory DC
	memDC = CreateCompatibleDC(hDC);

	// Select a compatible bitmap into the memory DC
	bitmap = CreateCompatibleBitmap(hDC, rcClient.right - rcClient.left,
	                                rcClient.bottom - rcClient.top);
	hOldBitmap = SelectObject(memDC, bitmap);

	// First let the control do its default drawing.
	CallWindowProc(g_lpTreeWndProc, hWnd, uMsg, (WPARAM)memDC, 0);

	// Draw bitmap in the background

	{
		HPALETTE hPAL;		 
		HDC maskDC;
		HBITMAP maskBitmap;
		HDC tempDC;
		HDC imageDC;
		HBITMAP bmpImage;
		HBITMAP hOldBmpImage;
		HBITMAP hOldMaskBitmap;
		HBITMAP hOldHBitmap;
		int i, j;
		RECT rcRoot;

		// Now create a mask
		maskDC = CreateCompatibleDC(hDC);	
		
		// Create monochrome bitmap for the mask
		maskBitmap = CreateBitmap(rcClient.right - rcClient.left,
		                          rcClient.bottom - rcClient.top, 
		                          1, 1, NULL);

		hOldMaskBitmap = SelectObject(maskDC, maskBitmap);
		SetBkColor(memDC, GetSysColor(COLOR_WINDOW));

		// Create the mask from the memory DC
		BitBlt(maskDC, 0, 0, rcClient.right - rcClient.left,
		       rcClient.bottom - rcClient.top, memDC, 
		       rcClient.left, rcClient.top, SRCCOPY);

		tempDC = CreateCompatibleDC(hDC);
		hOldHBitmap = SelectObject(tempDC, hBackground);

		imageDC = CreateCompatibleDC(hDC);
		bmpImage = CreateCompatibleBitmap(hDC,
		                                  rcClient.right - rcClient.left, 
		                                  rcClient.bottom - rcClient.top);
		hOldBmpImage = SelectObject(imageDC, bmpImage);

		hPAL = GetBackgroundPalette();
		if (hPAL == NULL)
			hPAL = CreateHalftonePalette(hDC);

		if (GetDeviceCaps(hDC, RASTERCAPS) & RC_PALETTE && hPAL != NULL)
		{
			SelectPalette(hDC, hPAL, FALSE);
			RealizePalette(hDC);
			SelectPalette(imageDC, hPAL, FALSE);
		}
		
		// Get x and y offset
		TreeView_GetItemRect(hWnd, TreeView_GetRoot(hWnd), &rcRoot, FALSE);
		rcRoot.left = -GetScrollPos(hWnd, SB_HORZ);

		// Draw bitmap in tiled manner to imageDC
		for (i = rcRoot.left; i < rcClient.right; i += bmDesc->bmWidth)
			for (j = rcRoot.top; j < rcClient.bottom; j += bmDesc->bmHeight)
				BitBlt(imageDC,  i, j, bmDesc->bmWidth, bmDesc->bmHeight, 
				       tempDC, 0, 0, SRCCOPY);

		// Set the background in memDC to black. Using SRCPAINT with black and any other
		// color results in the other color, thus making black the transparent color
		SetBkColor(memDC, RGB(0,0,0));
		SetTextColor(memDC, RGB(255,255,255));
		BitBlt(memDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
		       rcClip.bottom - rcClip.top,
		       maskDC, rcClip.left, rcClip.top, SRCAND);

		// Set the foreground to black. See comment above.
		SetBkColor(imageDC, RGB(255,255,255));
		SetTextColor(imageDC, RGB(0,0,0));
		BitBlt(imageDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left, 
		       rcClip.bottom - rcClip.top,
		       maskDC, rcClip.left, rcClip.top, SRCAND);

		// Combine the foreground with the background
		BitBlt(imageDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
			   rcClip.bottom - rcClip.top, 
			   memDC, rcClip.left, rcClip.top, SRCPAINT);

		// Draw the final image to the screen
		BitBlt(hDC, rcClip.left, rcClip.top, rcClip.right - rcClip.left,
			   rcClip.bottom - rcClip.top, 
			   imageDC, rcClip.left, rcClip.top, SRCCOPY);
		
		SelectObject(maskDC, hOldMaskBitmap);
		SelectObject(tempDC, hOldHBitmap);
		SelectObject(imageDC, hOldBmpImage);

		DeleteDC(maskDC);
		DeleteDC(imageDC);
		DeleteDC(tempDC);
		DeleteObject(bmpImage);
		DeleteObject(maskBitmap);

		if (GetBackgroundPalette() == NULL)
		{
			DeleteObject(hPAL);
			hPAL = 0;
		}
	}

	SelectObject(memDC, hOldBitmap);
	DeleteObject(bitmap);
	DeleteDC(memDC);
	EndPaint(hWnd, &ps);
	ReleaseDC(hWnd, hDC);
}

/* Header code - Directional Arrows */
static LRESULT CALLBACK TreeWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (GetBackgroundBitmap() != NULL)
	{
		switch (uMsg)
		{
		case WM_MOUSEMOVE:
			{
				if (MouseHasBeenMoved())
					ShowCursor(TRUE);
				break;
			}

		case WM_KEYDOWN :
			if (wParam == VK_F2)
			{
				if (lpCurrentFolder->m_dwFlags & F_CUSTOM)
				{
					TreeView_EditLabel(hWnd,TreeView_GetSelection(hWnd));
					return TRUE;
				}
			}
			break;

		case WM_ERASEBKGND:
			return TRUE;
			break;

		case WM_PAINT:
			TreeCtrlOnPaint(hWnd, uMsg, wParam, lParam);
			UpdateWindow(hWnd);
			break;
		}
	}

	/* message not handled */
	return CallWindowProc(g_lpTreeWndProc, hWnd, uMsg, wParam, lParam);
}

/*
 * Filter code - should be moved to filter.c/filter.h
 * Added 01/09/99 - MSH <mhaaland@hypertech.com>
 */

/* find a FOLDERDATA by folderID */
LPFOLDERDATA FindFilter(DWORD folderID)
{
	int i;

	for (i = 0; g_lpFolderData[i].m_lpTitle; i++)
	{
		if (g_lpFolderData[i].m_nFolderId == folderID)
			return &g_lpFolderData[i];
	}
	return (LPFOLDERDATA) 0;
}

/***************************************************************************
    private structures
 ***************************************************************************/

/***************************************************************************
    private functions prototypes
 ***************************************************************************/

/***************************************************************************
    private functions
 ***************************************************************************/

/**************************************************************************/

static BOOL RegistExtraFolder(const char *name, LPEXFOLDERDATA *fExData, int msgcat, int icon, int subicon)
{
	char *title = strdup(name);
	char *ext = strrchr(title, '.');

	if (ext && *(ext + 1) && !mame_stricmp(ext + 1, "ini"))
	{
		*fExData = malloc(sizeof(EXFOLDERDATA));
		if (*fExData) 
		{
			*ext = '\0';

			assign_msg_catategory(msgcat, title);

			memset(*fExData, 0, sizeof(EXFOLDERDATA));

			strncpy((*fExData)->m_szTitle, title, 63);
			free(title);

			(*fExData)->m_nFolderId   = next_folder_id++;
			(*fExData)->m_nParent	  = -1;
			(*fExData)->m_dwFlags	  = F_CUSTOM;
			(*fExData)->m_nIconId	  = icon ? -icon : IDI_FOLDER;
			(*fExData)->m_nSubIconId  = subicon ? -subicon : IDI_FOLDER;

			return TRUE;
		}
	}

	free(title);

	return FALSE;
}

static int InitExtraFolders(void)
{
	struct stat        stat_buffer;
	struct _finddata_t files;
	int                count = 0;
	long               hLong;
	char               curdir[MAX_PATH];
	const char        *dir = GetFolderDir();

	memset(ExtraFolderData, 0, MAX_EXTRA_FOLDERS * sizeof(LPEXFOLDERDATA));

	/* NPW 9-Feb-2003 - MSVC stat() doesn't like stat() called with an empty
	 * string
	 */
	if (dir[0] == '\0')
		dir = ".";

	if (stat(dir, &stat_buffer) != 0)
	{
		_mkdir(dir);
	}

	_getcwd(curdir, MAX_PATH);

	_chdir(dir);

	hLong = _findfirst("*", &files);

	memset(ExtraFolderIcons, 0, sizeof ExtraFolderIcons);
	numExtraIcons = 0;

	while (!_findnext(hLong, &files))
	{
		int rooticon, subicon;
		FILE *fp;
		char buf[256];

		if ((files.attrib & _A_SUBDIR) != 0)
			continue;

		if ((fp = fopen(files.name, "r")) == NULL)
			continue;

		rooticon = 0;
		subicon = 0;

		while (fgets(buf, 256, fp))
		{
			char *p;

			if (buf[0] != '[')
				continue;

			p = strchr(buf, ']');
			if (p == NULL)
				continue;

			*p = '\0';
			if (!strcmp(&buf[1], "FOLDER_SETTINGS"))
			{
				while (fgets(buf, 256, fp))
				{
					char *name = strtok(buf, " =\r\n");
					if (name == NULL)
						break;

					if (!strcmp(name, "RootFolderIcon"))
					{
						name = strtok(NULL, " =\r\n");
						if (name != NULL)
							SetExtraIcons(name, &rooticon);
					}

					if (!strcmp(name, "SubFolderIcon"))
					{
						name = strtok(NULL, " =\r\n");
						if (name != NULL)
							SetExtraIcons(name, &subicon);
					}
				}
				break;
			}
		}
		fclose(fp);

		if (RegistExtraFolder(files.name, &ExtraFolderData[count], UI_MSG_EXTRA + count, rooticon, subicon))
			count++;
	}

	_chdir(curdir);
	return count;
}

void FreeExtraFolders(void)
{
	int i;

	for (i = 0; i < numExtraFolders; i++)
	{
		if (ExtraFolderData[i])
		{
			free(ExtraFolderData[i]);
			ExtraFolderData[i] = NULL;
		}
	}

	for (i = 0; i < numExtraIcons; i++)
	{
		free(ExtraFolderIcons[i]);
	}

	numExtraIcons = 0;

}


static void SetExtraIcons(char *name, int *id)
{
	char *p = strchr(name, '.');

	if (p != NULL)
		*p = '\0';

	ExtraFolderIcons[numExtraIcons] = malloc(strlen(name) + 1);
	if (ExtraFolderIcons[numExtraIcons])
	{
		*id = ICON_MAX + numExtraIcons;
		strcpy(ExtraFolderIcons[numExtraIcons], name);
		numExtraIcons++;
	}
}


// Called to add child folders of the top level extra folders already created
BOOL TryAddExtraFolderAndChildren(int parent_index)
{
	FILE*   fp = NULL;
	char    fname[MAX_PATH];
	char    readbuf[256];
	char*   p;
	char*   name;
	int     id, current_id;
	LPTREEFOLDER lpTemp = NULL;
	LPTREEFOLDER lpFolder = treeFolders[parent_index];

	current_id = lpFolder->m_nFolderId;

	id = lpFolder->m_nFolderId - MAX_FOLDERS;

	/* "folder\title.ini" */

	sprintf( fname, "%s\\%s.ini", 
	GetFolderDir(), 
	ExtraFolderData[id]->m_szTitle);

	fp = fopen(fname, "r");
	if (fp == NULL)
		return FALSE;

	while ( fgets(readbuf, 256, fp) )
	{
		/* do we have [...] ? */

		if (readbuf[0] == '[')
		{
			p = strchr(readbuf, ']');
			if (p == NULL)
			{
				continue;
			}

			*p = '\0';
			name = &readbuf[1];
	 
			/* is it [FOLDER_SETTINGS]? */

			if (strcmp(name, "FOLDER_SETTINGS") == 0)
			{
				current_id = -1;
				continue;
			}
			else
			{
				/* it it [ROOT_FOLDER]? */

				if (!strcmp(name, "ROOT_FOLDER"))
				{
					current_id = lpFolder->m_nFolderId;
					lpTemp = lpFolder;

				}
				else
				{
					/* must be [folder name] */

					current_id = next_folder_id++;
					/* create a new folder with this name,
					   and the flags for this folder as read from the registry */
					lpTemp = NewFolder(name,UI_MSG_EXTRA + (ExtraFolderData[id]->m_nFolderId - MAX_FOLDERS),TRUE,current_id,parent_index,
					                   ExtraFolderData[id]->m_nSubIconId,
					                   GetFolderFlags(name) | F_CUSTOM);

					AddFolder(lpTemp);
				}
			}
		}
		else if (current_id != -1)
		{
			/* string on a line by itself -- game name */

			name = strtok(readbuf, " \t\r\n");
			if (name == NULL)
			{
				current_id = -1;
				continue;
			}

			/* IMPORTANT: This assumes that all driver names are lowercase! */
			strlwr( name );

			if (lpTemp == NULL)
			{
				ErrorMsg("Error parsing %s: missing [folder name] or [ROOT_FOLDER]",
						 fname);
				current_id = lpFolder->m_nFolderId;
				lpTemp = lpFolder;
			}
			AddGame(lpTemp,GetGameNameIndex(name));
		}
	}

	if ( fp )
	{
		fclose( fp );
	}

	return TRUE;
}


void GetFolders(TREEFOLDER ***folders,int *num_folders)
{
	*folders = treeFolders;
	*num_folders = numFolders;
}

BOOL TryRenameCustomFolderIni(LPTREEFOLDER lpFolder,const char *old_name,const char *new_name)
{
	char filename[MAX_PATH];
	char new_filename[MAX_PATH];
	LPTREEFOLDER lpParent = NULL;
	if (lpFolder->m_nParent >= 0)
	{
		//it is a custom SubFolder
		lpParent = GetFolder( lpFolder->m_nParent );
		if( lpParent )
		{
			snprintf(filename,sizeof(filename),"%s\\%s\\%s.ini",GetIniDir(),lpParent->m_lpTitle, old_name );
			snprintf(new_filename,sizeof(new_filename),"%s\\%s\\%s.ini",GetIniDir(),lpParent->m_lpTitle, new_name );
			MoveFileA(filename,new_filename);
		}
	}
	else
	{
		//Rename the File, if it exists
		snprintf(filename,sizeof(filename),"%s\\%s.ini",GetIniDir(),old_name );
		snprintf(new_filename,sizeof(new_filename),"%s\\%s.ini",GetIniDir(), new_name );
		MoveFileA(filename,new_filename);
		//Rename the Directory, if it exists
		snprintf(filename,sizeof(filename),"%s\\%s",GetIniDir(),old_name );
		snprintf(new_filename,sizeof(new_filename),"%s\\%s",GetIniDir(), new_name );
		MoveFileA(filename,new_filename);
	}
	return TRUE;
}

BOOL TryRenameCustomFolder(LPTREEFOLDER lpFolder,const char *new_name)
{
	BOOL retval;
	char filename[MAX_PATH];
	char new_filename[MAX_PATH];
	
	if (lpFolder->m_nParent >= 0)
	{
		// a child extra folder was renamed, so do the rename and save the parent

		// save old title
		char *old_title = lpFolder->m_lpTitle;

		// set new title
		lpFolder->m_lpTitle = (char *)malloc(strlen(new_name) + 1);
		strcpy(lpFolder->m_lpTitle,new_name);

		// try to save
		if (TrySaveExtraFolder(lpFolder) == FALSE)
		{
			// failed, so free newly allocated title and restore old
			free(lpFolder->m_lpTitle);
			lpFolder->m_lpTitle = old_title;
			return FALSE;
		}
		TryRenameCustomFolderIni(lpFolder, old_title, new_name);
		// successful, so free old title
		free(old_title);
		return TRUE;
	}
	
	// a parent extra folder was renamed, so rename the file

	snprintf(new_filename,sizeof(new_filename),"%s\\%s.ini",GetFolderDir(),new_name);
	snprintf(filename,sizeof(filename),"%s\\%s.ini",GetFolderDir(),lpFolder->m_lpTitle);

	retval = MoveFileA(filename,new_filename);

	if (retval)
	{
		TryRenameCustomFolderIni(lpFolder, lpFolder->m_lpTitle, new_name);
		free(lpFolder->m_lpTitle);
		lpFolder->m_lpTitle = (char *)malloc(strlen(new_name) + 1);
		strcpy(lpFolder->m_lpTitle,new_name);
	}
	else
	{
		char buf[500];
		snprintf(buf,sizeof(buf),_UI("Error while renaming custom file %s to %s"),
				 filename,new_filename);
		MessageBox(GetMainWindow(), _Unicode(_UI(buf)), TEXT_MAME32NAME, MB_OK | MB_ICONERROR);
	}
	return retval;
}

void AddToCustomFolder(LPTREEFOLDER lpFolder,int driver_index)
{
	if ((lpFolder->m_dwFlags & F_CUSTOM) == 0)
	{
	    MessageBox(GetMainWindow(),_Unicode(_UI("Unable to add game to non-custom folder")),
				   TEXT_MAME32NAME,MB_OK | MB_ICONERROR);
		return;
	}

	if (TestBit(lpFolder->m_lpGameBits,driver_index) == 0)
	{
		AddGame(lpFolder,driver_index);
		if (TrySaveExtraFolder(lpFolder) == FALSE)
			RemoveGame(lpFolder,driver_index); // undo on error
	}
}

void RemoveFromCustomFolder(LPTREEFOLDER lpFolder,int driver_index)
{
	if ((lpFolder->m_dwFlags & F_CUSTOM) == 0)
	{
	    MessageBox(GetMainWindow(),_Unicode(_UI("Unable to remove game from non-custom folder")),
				   TEXT_MAME32NAME,MB_OK | MB_ICONERROR);
		return;
	}

	if (TestBit(lpFolder->m_lpGameBits,driver_index) != 0)
	{
		RemoveGame(lpFolder,driver_index);
		if (TrySaveExtraFolder(lpFolder) == FALSE)
			AddGame(lpFolder,driver_index); // undo on error
	}
}

BOOL TrySaveExtraFolder(LPTREEFOLDER lpFolder)
{
	char fname[MAX_PATH];
	FILE *fp;
	BOOL error = FALSE;
	int i,j;

	LPTREEFOLDER root_folder = NULL;
	LPEXFOLDERDATA extra_folder = NULL;

	for (i=0;i<numExtraFolders;i++)
	{
		if (ExtraFolderData[i]->m_nFolderId == lpFolder->m_nFolderId)
		{
			root_folder = lpFolder;
			extra_folder = ExtraFolderData[i];
			break;
		}

		if (lpFolder->m_nParent >= 0 &&
			ExtraFolderData[i]->m_nFolderId == treeFolders[lpFolder->m_nParent]->m_nFolderId)
		{
			root_folder = treeFolders[lpFolder->m_nParent];
			extra_folder = ExtraFolderData[i];
			break;
		}
	}

	if (extra_folder == NULL || root_folder == NULL)
	{
	   MessageBox(GetMainWindow(), _Unicode(_UI("Error finding custom file name to save")),	TEXT_MAME32NAME, MB_OK | MB_ICONERROR);
	   return FALSE;
	}
	/* "folder\title.ini" */

	snprintf( fname, sizeof(fname), "%s\\%s.ini", GetFolderDir(), extra_folder->m_szTitle);

	fp = fopen(fname, "wt");
	if (fp == NULL)
	   error = TRUE;
	else
	{
	   TREEFOLDER *folder_data;


	   fprintf(fp,"[FOLDER_SETTINGS]\n");
	   // negative values for icons means it's custom, so save 'em
	   if (extra_folder->m_nIconId < 0)
	   {
		   fprintf(fp, "RootFolderIcon %s\n",
				   ExtraFolderIcons[(-extra_folder->m_nIconId) - ICON_MAX]);
	   }
	   if (extra_folder->m_nSubIconId < 0)
	   {
		   fprintf(fp,"SubFolderIcon %s\n",
				   ExtraFolderIcons[(-extra_folder->m_nSubIconId) - ICON_MAX]);
	   }

	   /* need to loop over all our TREEFOLDERs--first the root one, then each child.
		  start with the root */

	   folder_data = root_folder;

	   fprintf(fp,"\n[ROOT_FOLDER]\n");

	   for (i=0;i<GetNumGames();i++)
	   {
		   int driver_index = GetIndexFromSortedIndex(i); 
		   if (TestBit(folder_data->m_lpGameBits,driver_index))
		   {
			   fprintf(fp,"%s\n",drivers[driver_index]->name);
		   }
	   }

	   /* look through the custom folders for ones with our root as parent */
	   for (j=0;j<numFolders;j++)
	   {
		   folder_data = treeFolders[j];

		   if (folder_data->m_nParent >= 0 &&
			   treeFolders[folder_data->m_nParent] == root_folder)
		   {
			   fprintf(fp,"\n[%s]\n",folder_data->m_lpTitle);
			   
			   for (i=0;i<GetNumGames();i++)
			   {
				   int driver_index = GetIndexFromSortedIndex(i); 
				   if (TestBit(folder_data->m_lpGameBits,driver_index))
				   {
					   fprintf(fp,"%s\n",drivers[driver_index]->name);
				   }
			   }
		   }
	   }
	   if (fclose(fp) != 0)
		   error = TRUE;
	}

	if (error)
	{
		char buf[500];
		snprintf(buf,sizeof(buf),_UI("Error while saving custom file %s"),fname);
		MessageBox(GetMainWindow(), _Unicode(_UI(buf)), TEXT_MAME32NAME, MB_OK | MB_ICONERROR);
	}
	return !error;
}

HIMAGELIST GetTreeViewIconList(void)
{
	return hTreeSmall;
}

int GetTreeViewIconIndex(int icon_id)
{
	int i;

	if (icon_id < 0)
		return -icon_id;

	for (i = 0; i < sizeof(treeIconNames) / sizeof(treeIconNames[0]); i++)
	{
		if (icon_id == treeIconNames[i].nResourceID)
			return i;
	}

	return -1;
}

/* End of source file */
