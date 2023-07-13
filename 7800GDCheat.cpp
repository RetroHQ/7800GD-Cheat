#include <stdio.h>
#include <string.h>
#include "Types.h"
#include "crc32.h"
#include "cheats.h"

#ifdef WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

#ifdef _MSC_VER 
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

////////////////////////////////////////////////////////////////////////////////
//
// Program usage
//
////////////////////////////////////////////////////////////////////////////////

void ShowUsage(const char *exe)
{
	// strip absolute path
	if (strrchr(exe, PATH_SEPARATOR))
	{
		exe = strrchr(exe, PATH_SEPARATOR) + 1;
	}

	// show usage
	printf(
		"Usage: %s [command] ...\n\n"
		"Command:\n"
		"crc [file.a78]    Generate CRC for ROM data\n"
		"parse [file.cht]  Parse the cheat file for errors\n", exe);
}

////////////////////////////////////////////////////////////////////////////////
//
// Command line interface
//
////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char **argv)
{
	if (argc < 2)
	{
		ShowUsage(argv[0]);
		return -1;
	}

	for (int n = 1; n < argc; n++)
	{
		const char *pCmd = argv[n];
		if (strcasecmp(pCmd, "crc") == 0 ||
			strcasecmp(pCmd, "parse") == 0)
		{
			// want a filename
			if ((n + 1) < argc)
			{
				const char *pFilename = argv[++n];

				// crc
				if (strcasecmp(pCmd, "crc") == 0)
				{
					GenerateA78CRC(pFilename);
				}

				// parse
				else
				{
					u32 nCheats;
					u32 errors = ParseCheats(pFilename, &nCheats);
					printf("Parsed '%s', %d cheats, %d errors found.\n", pFilename, nCheats, errors);
				}
			}

			// no file given
			else
			{
				printf("No filename given for '%s'.\n", pCmd);
			}
		}
	}

	return 0;
}
