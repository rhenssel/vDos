#ifndef VDOS_SUPPORT_H
#define VDOS_SUPPORT_H

#include <string.h>
#include <string>
#include <ctype.h>
#include "vDos.h"

#if defined (_MSC_VER)						// MS Visual C++
#define	strcasecmp(a,b) stricmp(a,b)
#define strncasecmp(a,b,n) _strnicmp(a,b,n)
#endif

#define safe_strncpy(a,b,n) do { strncpy((a),(b),(n)-1); (a)[(n)-1] = 0; } while (0)

char *ltrim(char *str);
char *rtrim(char *str);
char *trim(char * str);
char * upcase(char * str);
char * lowcase(char * str);

bool ScanCMDBool(char * cmd,char const * const check);
char * ScanCMDRemain(char * cmd);
char * StripWord(char *&cmd);

void upcase(std::string &str);
void lowcase(std::string &str);

extern Bit16u cpMap[];

int Unicode2Ascii(Bit16u *unicode, Bit8u *ascii, int length);

#endif
