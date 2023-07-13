#include <stdio.h>
#include <string.h>
#include "types.h"

#ifdef _MSC_VER 
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

enum ECheatOptionType
{
	ECheatOptionType_UnsignedByte = 0,
	ECheatOptionType_SignedByte,
	ECheatOptionType_Enum,
	ECheatOptionType_INVALID
};

enum EProgramType
{
	EProgramType_Off = 0,
	EProgramType_On,
	EProgramType_Frame,
	EProgramType_INVALID
};

bool ReadCheatLine(char *out, u32 maxRead, FILE *file, u32 *errorPos)
{
	bool comment = false;
	bool instring = false;
	char c;
	u32 linePos = 0;
	maxRead--;

	// read a character at a time
	while (1)
	{
		c = fgetc(file);
		if (feof(file))
		{
			break;
		}
		linePos++;

		// ignore space (outside of strings) cr and tab
		if ((!instring && c == 32) || c == 13 || c == 9) 
		{
			continue;
		}
		
		// start / end of string
		if (c == '[')
		{
			if (instring)
			{
				*errorPos = linePos;
				return false;
			}
			instring = true;
		}
		if (c == ']')
		{
			if (!instring)
			{
				*errorPos = linePos;
				return false;
			}
			instring = false;
		}
		
		// if we find a semicolon then ignore the rest of the line
		if (c == ';')
		{
			comment = true;
			continue;
		}
		
		// lf terminates the line read
		if (c == 10)
		{
			break;
		}

		// keep character we want
		if (!comment && maxRead)
		{
			*out++ = c;
			maxRead--;
		}
	}
	
	*out = 0;
	*errorPos = 0;
	
	return maxRead != 0 && !instring;
}

char *ParseNumber(u32 *out, char *in)
{
	u32 val = 0;
	u32 base = 10;

	// first character can be $ or % to set base
	char c = *in;
	if (c == '$' || c == '%')
	{
		in++;
		if (c == '$') base = 16;
		else base = 2;
	}
	
	while (*in)
	{
		// get character and force lowercase
		c = *in | 0x20;
		
		// valid hex characters
		if (c >= '0' && c <= '1') c = c - '0';
		else if (base >= 10 && c >= '2' && c <= '9') c = c - '0';
		else if (base == 16 && c >= 'a' && c <= 'f') c = c - ('a' - 10);
		// invalid, end of number (may be a range, like 0-10, so 0 is still valid)
		else break;
		
		in++;
		
		val = (val * base) + c;
	}
	
	if (out) *out = val;
	
	return in;
}

u32 GetStringLength(const char *p)
{
	if (*p++ != '[') return 0;
	char c;
	u32 len = 0;
	while ((c = *p++))
	{
		if (c == ']') break;
		len++;
	}
	
	return len;
}

char *FindParamType(ECheatOptionType *pType, char *pStr)
{
	ECheatOptionType nType = ECheatOptionType_INVALID;
	char c = *pStr++ | 0x20;
	
	u8 nSigned = 0xff;
	if (c == 'u' || c == 's')
	{
		nSigned = c == 's' ? 1 : 0;
		c = *pStr++ | 0x20;
	}
	
	switch (c)
	{
/*	case 'e':
		if (nSigned != 0xff)
		{
			nType = ECheatOptionType_Enum;
		}
		break;*/
		
	case 'b':
		nType = nSigned == 1 ? ECheatOptionType_SignedByte : ECheatOptionType_UnsignedByte;
		break;
	}
	
	if (pType)
	{
		*pType = nType;
	}
	
	return pStr;
}

char *FindParamRange(u8 *pMin, u8 *pMax, bool *pError, char *pStr)
{
	bool bError = false;
	u32 min = 0, max = 0xff;
	if (*pStr == '{')
	{
		pStr++;
	
		u32 num1, num2;
		pStr = ParseNumber(&num1, pStr);
		bError = *pStr++ != '-';

		if (!bError)
		{
			pStr = ParseNumber(&num2, pStr);
			
			bError = *pStr++ != '}';
			
			if (!bError)
			{
				if (num1 < num2)
				{
					min = num1;
					max = num2;
				}
				else
				{
					min = num2;
					max = num1;
				}
			}
		}
	}
	
	*pMin = min;
	*pMax = max;
	*pError = bError;
	
	return pStr;
}

EProgramType GetProgramType(const char *pStr)
{
	// these match EProgramType in order
	static const char *aType[] = { "OFF", "ON", "FRAME" };
	for (u32 n = 0; n < sizeof(aType)/4; n++)
	{
		if (strcasecmp(aType[n], pStr) == 0)
		{
			return (EProgramType)n;
		}
	}
	return EProgramType_INVALID;
}

void ShowErrorContext(const char *pLine, int nChar)
{
	// show upto 16 characters before the bad character
	int start = nChar - 16;
	if (start < 0) start = 0;

	// show a max of 32 characters
	printf("%.32s\n", pLine + start);

	// mark the bad character
	while (--nChar) printf(" ");
	printf("^\n");
}

u32 ParseCheats(const char *pFilename, u32 *pCheats)
{
	FILE *file;
	char szLine[256];
	u32 linePos = 0;
	bool inCheat = false;
	bool inProgram = false;
	u32 nCheatDefined = 0;
	u32 nErrors = 0;
	u32 nCheats = 0;
	
	if (fopen_s(&file, pFilename, "rt") != 0)
	{
		printf("Unable to open '%s' for reading.\n", pFilename);
		*pCheats = 0;
		return 0;
	}

	while (!feof(file))
	{
		u32 errorPos;
		linePos++;
		// read one line stripping whitespace
		if (!ReadCheatLine(szLine, 256, file, &errorPos))
		{
			if (errorPos)
			{
				printf("Line(%d:%d): unexpected character, line skipped.\n", linePos, errorPos);

				szLine[errorPos] = 0;
				ShowErrorContext(szLine, errorPos);
				nErrors++;
			}
			else
			{
				printf("Line(%d): too long or missing ], line skipped.\n", linePos);
			}

			continue;
		}

		// skip blank lines
		if (szLine[0] == 0) continue;

		// different things valid in a cheat definition block
		if (inProgram)
		{
			// on next option, program or cheat start, exit parsing program
			if (szLine[0] == '[' || szLine[0] == ':' || szLine[0] == '#')
			{
				inProgram = false;
			}
			else
			{
				// process line
				char *p = szLine;
				while (*p)
				{
					// get character
					const char c = *p++;
			
					// set address
					if (c == '@')
					{
						// check for low RAM writes, these are in 7800 address space
						if (*p == '<')
						{
							p++;
						}

						u32 addr;
						p = ParseNumber(&addr, p);
					}
			
					// write to current address pointer
					// can be multiple writes with comma separated list
					else if (c == '=')
					{
						bool expectingData = true;
						while (*p)
						{
							bool bUnexpected = false;
							
							// parameter write
							if (*p == '[')
							{
								bUnexpected = !expectingData;
								errorPos = (p - szLine) + 1;
								
								u32 len = GetStringLength(p);
								p += len + 2;
								expectingData = false;
							}

							// list of items
							else if (*p == ',')
							{
								bUnexpected = expectingData;
								errorPos = (p - szLine) + 1;
								expectingData = true;
								p++;
							}

							// otherwise assume constant write
							else
							{
								bUnexpected = !expectingData;
								errorPos = (p - szLine) + 1;

								u32 val;
								p = ParseNumber(&val, p);
								expectingData = false;
							}

							if (bUnexpected)
							{
								errorPos = p - szLine;
								printf("Line(%d:%d): unexpected character in data list.\n", linePos, errorPos);
								ShowErrorContext(szLine, errorPos);
								nErrors++;
							}
						}
					}
					// invalid chatacter
					else
					{
						errorPos = p - szLine;
						printf("Line(%d:%d): unexpected character in cheat block.\n", linePos, errorPos);
						ShowErrorContext(szLine, errorPos);
						nErrors++;
					}
				}

				continue;
			}
		}

		// cheat start
		if (szLine[0] == '#')
		{
			// check the CRC(s) are OK
			u32 numcrc = 0;
			char *crc = szLine+1;
			while (*crc)
			{
				u32 crcno;
				crc = ParseNumber(&crcno, crc);
				if (*crc)
				{
					if (*crc != ',')
					{
						errorPos = (crc - szLine) + 1;
						printf("Line(%d:%d): unexpected character in CRC list.\n", linePos, errorPos);
						ShowErrorContext(szLine, errorPos);
						nErrors++;
						break;
					}
					crc++;
				}
				numcrc++;
			}
			if (numcrc == 0)
			{
				errorPos = 2;
				printf("Line(%d:%d): CRC list empty.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
			}
			nCheats++;
		}

		// cheat option
		else if (szLine[0] == '[')
		{
			// check the option line is not malformed
			u32 len = GetStringLength(szLine);
			if (szLine[len+2] != 0)
			{
				errorPos = len+3;
				printf("Line(%d:%d): unexpected character.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
			}
			nCheatDefined = 0;
		}

		// parameter
		else if (szLine[0] == '?')
		{
			// check parameter string is ok
			u32 len = GetStringLength(szLine+1);
			if (szLine[1] != '[')
			{
				errorPos = 2;
				printf("Line(%d:%d): option name missing ?[...], line skipped.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
				continue;
			}

			// next is the type details, requires = first
			char *p = szLine + len + 3;
			if (*p++ != '=')
			{
				errorPos = len + 4;
				printf("Line(%d:%d): parameter type missing.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
				continue;
			}
									
			// now actual type, can be --
			// [U/S]B	[un]signed byte
			// E		enum
							
			errorPos = (p - szLine) + 1; // possible error pos
			ECheatOptionType eType;
			p = FindParamType(&eType, p);
			if (eType == ECheatOptionType_INVALID)
			{
				printf("Line(%d:%d): invalid parameter type.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
				continue;
			}

			// if type is valid then check for further details
			switch (eType)
			{
			// signed and unsigned bytes can have a range
			case ECheatOptionType_UnsignedByte:
			case ECheatOptionType_SignedByte:
				{
					u8 min, max;
					bool bError = false;
					p = FindParamRange(&min, &max, &bError, p);
					errorPos = p - szLine;

					if (!bError && *p)
					{
						if ((bError = *p++ != ':'))
						{
							errorPos = p - szLine;
						}

						else
						{
							u32 num;
							p = ParseNumber(&num, p);

							if (*p)
							{
								errorPos = (p - szLine) + 1;
							}
						}
					}
					if (bError)
					{
						printf("Line(%d:%d): invalid character in parameter range.\n", linePos, errorPos);
						ShowErrorContext(szLine, errorPos);
						nErrors++;
					}
				}
				break;
			}
		}
								
		// actions
		else if (szLine[0] == ':')
		{
			// see if we have a valid cheat type
			EProgramType eProgType = GetProgramType(szLine + 1);
			if (eProgType == EProgramType_INVALID)
			{
				errorPos = 2;
				printf("Line(%d:%d): invalid cheat type.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
				continue;
			}

			inProgram = true;

			// check it's not already defined
			if ((nCheatDefined & (1 << (u32)eProgType)) != 0)
			{
				errorPos = 1;
				printf("Line(%d:%d): cheat type is already defined for this option.\n", linePos, errorPos);
				ShowErrorContext(szLine, errorPos);
				nErrors++;
				continue;
			}
			nCheatDefined |= 1 << (u32)eProgType;
		}
		else
		{
			errorPos = 1;
			printf("Line(%d:%d): unexpected character.\n", linePos, errorPos);
			ShowErrorContext(szLine, errorPos);
			nErrors++;
		}
	}
	*pCheats = nCheats;
	return nErrors;
}
