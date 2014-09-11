#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <string>
  
#include "vDos.h"
#include "support.h"

// codepage is standard 437, only ascii 128 = Euro
Bit16u cpMap[256] = {
	0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2219, 0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
	0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
	0x20ac, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,		// 176 - 223 line/box drawing
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
	};


int Unicode2Ascii(Bit16u *unicode, Bit8u *ascii, int maxLength)		// Unicode is exspected to end with 0, the number of translated characters (length of ascii) is returned
	{
	int	translated;
	for (translated = 0; translated < maxLength && *unicode; translated++)
		{
		if (*unicode >= 0x20 && *unicode <= 0x7e)					// These are equal and most frequent
			ascii[translated] = *((Bit8u *)unicode);
		else if (*unicode == 9)										// Tab -> tab
			ascii[translated] = 9;
		else if (*unicode == 13)									// Return -> return
			{
			ascii[translated] = 13;
			if (unicode[1] == 10)									// Linefeed, but that should always come after return
				unicode++;
			}
		else														// try to translate to ASCII > 0x7e
			{														// so ASCII 0-31 are dropped, don't make sense for text pasting?
			ascii[translated] = 127;								// Untranslated become underscores
			for (int j = 0x7f; j < 256; j++)						// slow, in time based upon a table?
				if (*unicode == cpMap[j])
					{
					ascii[translated++] = j;
					break;
					}
			translated--;
			}
		unicode++;
		}
	return translated;
	}

void upcase(std::string &str)
	{
	int (*tf)(int) = std::toupper;
	std::transform(str.begin(), str.end(), str.begin(), tf);
	}

void lowcase(std::string &str)
	{
	int (*tf)(int) = std::tolower;
	std::transform(str.begin(), str.end(), str.begin(), tf);
	}
 
//	Ripped some source from freedos for this one.
char *ltrim(char *str)
	{ 
	while (isspace(*str))
		str++;
	return str;
	}

char *rtrim(char *str)
	{
	char *p;
	p = strchr(str, '\0');
	while (--p >= str && isspace(*p)) {};
	p[1] = '\0';
	return str;
	}

char *trim(char *str)
	{
	return ltrim(rtrim(str));
	}

char * upcase(char * str)
	{
    for (char* idx = str; *idx; idx++)
		*idx = toupper(*reinterpret_cast<unsigned char*>(idx));
    return str;
	}

char * lowcase(char * str)
	{
	for (char* idx = str; *idx; idx++)
		*idx = tolower(*reinterpret_cast<unsigned char*>(idx));
	return str;
	}

bool ScanCMDBool(char * cmd, char const * const check)
	{
	char * scan = cmd;
	size_t c_len = strlen(check);
	while ((scan = strchr(scan, '/')))
		{
		// found a / now see behind it
		scan++;
		if (strnicmp(scan, check, c_len) == 0 && (scan[c_len] == ' ' || scan[c_len] == '\t' || scan[c_len]== '/' || scan[c_len] == 0))
			{
			// Found a math now remove it from the string
			memmove(scan-1, scan+c_len, strlen(scan+c_len)+1);
			trim(scan-1);
			return true;
			}
		}
	return false;
	}

// This scans the command line for a remaining switch and reports it else returns 0
char * ScanCMDRemain(char * cmd)
	{
	char * scan, *found;;
	if ((scan = found = strchr(cmd, '/')))
		{
		while (*scan && !isspace(*scan))
			scan++;
		*scan = 0;
		return found;
		}
	else
		return 0; 
	}

char * StripWord(char *&line)
	{
	char * scan = ltrim(line);
	if (*scan == '"')
		if (char * end_quote = strchr(++scan, '"'))
			{
			*end_quote = 0;
			line = ltrim(++end_quote);
			return scan;
			}
	char * begin = scan;
	for (char c = *scan; (c = *scan); scan++)
		if (isspace(c))
			{ 
			*scan++ = 0;
			break;
			}
	line = scan;
	return begin;
	}

static char buf[1024];
void E_Exit(const char * format,...)
	{
	va_list msg;
	va_start(msg, format);
	vsprintf(buf, format, msg);
	va_end(msg);
	strcat(buf, "\n");
	throw(buf);
	}
