/***************************************************************************

    makelist.c

    Create and sort the driver list.

****************************************************************************

    Copyright Aaron Giles
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

        * Redistributions of source code must retain the above copyright
          notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright
          notice, this list of conditions and the following disclaimer in
          the documentation and/or other materials provided with the
          distribution.
        * Neither the name 'MAME' nor the names of its contributors may be
          used to endorse or promote products derived from this software
          without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY AARON GILES ''AS IS'' AND ANY EXPRESS OR
    IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL AARON GILES BE LIABLE FOR ANY DIRECT,
    INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
    STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "corefile.h"


#define MAX_DRIVERS 65536

static const char *drivlist[MAX_DRIVERS];
static int drivcount;

#ifdef DRIVER_SWITCH
static char filename[_MAX_FNAME];
static const char *extra_drivlist[MAX_DRIVERS];
static int extra_drivcount;
#endif /* DRIVER_SWITCH */


//-------------------------------------------------
//  driver_sort_callback - compare two items in
//  a string array
//-------------------------------------------------

int sort_callback(const void *elem1, const void *elem2)
{
	const char **item1 = (const char **)elem1;
	const char **item2 = (const char **)elem2;
	return strcmp(*item1, *item2);
}

int parse_file(const char *srcfile)
{
	// read source file
	void *buffer;
	UINT32 length;
	file_error filerr = core_fload(srcfile, &buffer, &length);
	if (filerr != FILERR_NONE)
	{
		fprintf(stderr, "Unable to read source file '%s'\n", srcfile);
		return 1;
	}

	// rip through it to find all drivers
	char *srcptr = (char *)buffer;
	char *endptr = srcptr + length;
	int linenum = 1;
	bool in_comment = false;
	bool in_import = false;
	while (srcptr < endptr)
	{
		char c = *srcptr++;

		// count newlines
		if (c == 13 || c == 10)
		{
			if (c == 13 && *srcptr == 10)
				srcptr++;
			linenum++;
			continue;
		}

		// skip any spaces
		if (isspace(c))
			continue;

		// look for end of C comment
		if (in_comment && c == '*' && *srcptr == '/')
		{
			srcptr++;
			in_comment = false;
			continue;
		}

		// skip anything else inside a C comment
		if (in_comment)
			continue;

		// look for start of C comment
		if (c == '/' && *srcptr == '*')
		{
			srcptr++;
			in_comment = true;
			continue;
		}

		// mamep: if we hit a preprocesser comment, scan to the end of line
		if (c == '#' && *srcptr == ' ')
		{
			while (srcptr < endptr && *srcptr != 13 && *srcptr != 10)
				srcptr++;
			continue;
		}

		// look for start of import directive start
		if (c == '#')
		{
			in_import = true;
			continue;
		}

		// if we hit a C++ comment, scan to the end of line
		if (c == '/' && *srcptr == '/')
		{
			while (srcptr < endptr && *srcptr != 13 && *srcptr != 10)
				srcptr++;
			continue;
		}

		if (in_import) {
			in_import = false;
			char filename[256];
			filename[0] = 0;
			srcptr--;
			for (int pos = 0; srcptr < endptr && pos < ARRAY_LENGTH(filename) - 1 && !isspace(*srcptr); pos++)
			{
				filename[pos] = *srcptr++;
				filename[pos+1] = 0;
			}
			fprintf(stderr, "Importing drivers from '%s'\n", filename);
			parse_file(filename);
		} else {
			// extract the driver name
			char drivname[32];
			drivname[0] = 0;
			srcptr--;
			for (int pos = 0; srcptr < endptr && pos < ARRAY_LENGTH(drivname) - 1 && !isspace(*srcptr); pos++)
			{
				drivname[pos] = *srcptr++;
				drivname[pos+1] = 0;
			}

			// verify the name as valid
			for (char *drivch = drivname; *drivch != 0; drivch++)
			{
				if ((*drivch >= 'a' && *drivch <= 'z') || (*drivch >= '0' && *drivch <= '9') || *drivch == '_')
					continue;
				fprintf(stderr, "%s:%d - Invalid character '%c' in driver \"%s\"\n", srcfile, linenum, *drivch, drivname);
				return 1;
			}

			// add it to the list
			char *name = (char *)malloc(strlen(drivname) + 1);
			strcpy(name, drivname);
			drivlist[drivcount++] = name;
#ifdef DRIVER_SWITCH
			extra_drivlist[extra_drivcount++] = name;
#endif /* DRIVER_SWITCH */
		}
	}
	return 0;
}
//-------------------------------------------------
//  main - primary entry point
//-------------------------------------------------

int main(int argc, char *argv[])
{
	// needs at least 1 argument
	if (argc < 2)
	{
		fprintf(stderr,
			"Usage:\n"
			"  makelist <source.lst>\n"
		);
		return 0;
	}

	bool header_outputed = false;
	drivcount = 0;

	// extract arguments
	for (int src_idx = 1; src_idx < argc; src_idx++)
	{

		// extract arguments
		const char *srcfile = argv[src_idx];

#ifdef DRIVER_SWITCH
		extra_drivcount = 0;
#endif /* DRIVER_SWITCH */
		{
			char drive[_MAX_DRIVE];
			char dir[_MAX_DIR];
			char file[_MAX_FNAME];
			char ext[_MAX_EXT];
			_splitpath(srcfile, drive, dir, file, ext);
#ifdef DRIVER_SWITCH
			strcpy(filename, file);
#endif /* DRIVER_SWITCH */

		if (stricmp(ext, ".lst")) continue;
		}

		if (parse_file(srcfile)) {
			return 1;
		}

#ifdef DRIVER_SWITCH
		// add a reference to the ___empty driver
		extra_drivlist[extra_drivcount++] = "___empty";

		// sort the list
		qsort(extra_drivlist, extra_drivcount, sizeof(*extra_drivlist), sort_callback);

		// start with a header
		if (!header_outputed)
		{
			printf("#include \"emu.h\"\n\n");
			header_outputed = true;
		}

		// output the list of externs first
		for (int index = 0; index < extra_drivcount; index++)
			printf("GAME_EXTERN(%s);\n", extra_drivlist[index]);
		printf("\n");

		// then output the array
		printf("const game_driver * const driver_switch::%sdrivers[%d] =\n", filename, extra_drivcount+1);
		printf("{\n");
		for (int index = 0; index < extra_drivcount; index++)
			printf("\t&GAME_NAME(%s)%s\n", extra_drivlist[index], ",");
		printf("\tNULL\n");
		printf("};\n");
		printf("\n");
#endif /* DRIVER_SWITCH */
	}

	// add a reference to the ___empty driver
	drivlist[drivcount++] = "___empty";

	// output a count
	if (drivcount == 0)
	{
		fprintf(stderr, "No drivers found\n");
		return 1;
	}
	fprintf(stderr, "%d drivers found\n", drivcount);

	// sort the list
	qsort(drivlist, drivcount, sizeof(*drivlist), sort_callback);

	// start with a header
	if (!header_outputed)
	printf("#include \"emu.h\"\n\n");

	// output the list of externs first
	for (int index = 0; index < drivcount; index++)
		printf("GAME_EXTERN(%s);\n", drivlist[index]);
	printf("\n");

	// then output the array
#ifdef DRIVER_SWITCH
	printf("const game_driver * driver_list::s_drivers_sorted[%d] =\n", drivcount);
#else
	printf("const game_driver * const driver_list::s_drivers_sorted[%d] =\n", drivcount);
#endif /* DRIVER_SWITCH */
	printf("{\n");
	for (int index = 0; index < drivcount; index++)
		printf("\t&GAME_NAME(%s)%s\n", drivlist[index], (index == drivcount - 1) ? "" : ",");
	printf("};\n");
	printf("\n");

	// also output a global count
	printf("int driver_list::s_driver_count = %d;\n", drivcount);

	return 0;
}