#ifndef VDOS_SUPPORT_H
#define VDOS_SUPPORT_H

#include <string.h>
#include <string>
#include <ctype.h>
#include "vDos.h"

#define safe_strncpy(a,b,n) do { strncpy((a),(b),(n)-1); (a)[(n)-1] = 0; } while (0)

char *lTrim(char *str);
char *rTrim(char *str);
char *rSpTrim(char *str);
char *lrTrim(char * str);
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
