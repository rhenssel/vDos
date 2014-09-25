#define _CRT_RAND_S

#include "devicePRT.h"
#include "process.h"
#include <Shellapi.h>
#include "vDos.h"
#include "support.h"
#include <stdarg.h>

#include "SDL.h"
#include "..\..\SDL-1.2.15\include\SDL_syswm.h"

void LPT_CheckTimeOuts(Bit32u mSecsCurr)
{
	for (int dn = 0; dn < DOS_DEVICES; dn++)
		if (Devices[dn] && Devices[dn]->timeOutAt != 0)
			if (Devices[dn]->timeOutAt <= mSecsCurr)
			{
		Devices[dn]->timeOutAt = 0;
		Devices[dn]->Close();
		return;																// One device per run cycle
			}
}

bool device_PRT::Read(Bit8u * data, Bit16u * size)
{
	*size = 0;
	return true;
}

bool device_PRT::Write(Bit8u * data, Bit16u * size)
{
	// Too rude: filter out ascii 0's, at least Multivers uses them and DOSprinter doesn't filter them out
	// At least not at determinating DPI with DPIA set
	// May we modify the original data? Move it to rawdata?
	Bit8u * datasrc = data;
	Bit8u * datadst = data;

	int numSpaces = 0;
	for (Bit16u idx = *size; idx; idx--)
	{
		if (*datasrc == 0x0c)
			ffWasLast = true;
		else if (!isspace(*datasrc))
			ffWasLast = false;
		if (*datasrc == ' ')														// Put spaces on hold
			numSpaces++;
		else
		{
			if (numSpaces && *datasrc != 0x0a && *datasrc != 0x0d)					// Spaces on hold and not end of line
			{
				while (numSpaces--)
					*(datadst++) = ' ';
				numSpaces = 0;
			}
			//			if (*datasrc)
			*(datadst++) = *datasrc;
		}
		datasrc++;
	}
	while (numSpaces--)
		*(datadst++) = ' ';
	if (Bit16u newsize = datadst - data)											// If data
	{
		if (rawdata.capacity() < 100000)											// Prevent repetive size allocations
			rawdata.reserve(100000);
		rawdata.append((char *)data, newsize);
#pragma warning(suppress: 28159)
		timeOutAt = GetTickCount() + LPT_LONGTIMEOUT;									// Long timeout so data is printed w/o Close()
	}
	return true;
}

void device_PRT::Close()
{
	rawdata.erase(rawdata.find_last_not_of(" \n\r\t") + 1);							// Remove trailing white space
	if (!rawdata.size())															// Nothing captured/to do
		return;
	int len = rawdata.size();
	if (len > 2 && rawdata[len - 3] == 0x0c && rawdata[len - 2] == 27 && rawdata[len - 1] == 64)	// <ESC>@ after last FF?
	{
		rawdata.erase(len - 2, 2);
		ffWasLast = true;
	}
	if (!ffWasLast && timeOutAt && !fastCommit)										// For programs initializing the printer in a seperate module
	{
#pragma warning(suppress: 28159)
		timeOutAt = GetTickCount() + LPT_SHORTTIMEOUT;								// Short timeout if ff was not last
		return;
	}
	CommitData();
}

void tryPCL2PDF(char * filename, bool postScript, bool openIt)
{
	char pcl6Path[512];																// Try to start gswin32c/pcl6 from where vDos was started
	strcpy(strrchr(strcpy(pcl6Path + 1, _pgmptr), '\\'), postScript ? "\\gswin32c.exe" : "\\pcl6.exe");
	if (_access(pcl6Path + 1, 4))														// If not found/readable
	{
		MessageBox(sdlHwnd, "Could not find pcl6 or gswin32c to handle printjob", "vDos - Error", MB_OK | MB_ICONWARNING);
		return;
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	ZeroMemory(&pi, sizeof(pi));

	pcl6Path[0] = '"';																// Surround path with quotes to be sure
	strcat(pcl6Path, "\" -sDEVICE=pdfwrite -o ");
	strcat(pcl6Path, filename);
	pcl6Path[strlen(pcl6Path) - 3] = 0;												// Replace .asc by .pdf
	strcat(pcl6Path, "pdf ");
	strcat(pcl6Path, filename);
	if (CreateProcess(NULL, pcl6Path, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))	// Start pcl6/gswin32c.exe
	{
		WaitForSingleObject(pi.hProcess, INFINITE);									// Wait for pcl6/gswin32c to exit
		DWORD exitCode = -1;
		GetExitCodeProcess(pi.hProcess, &exitCode);
		CloseHandle(pi.hProcess);													// Close process and thread handles
		CloseHandle(pi.hThread);
		if (exitCode != 0)
			MessageBox(sdlHwnd, "pcl6 or gswin32c could not convert printjob to PDF", "vDos - Error", MB_OK | MB_ICONWARNING);
		else if (openIt)
		{
			strcpy(pcl6Path, filename);
			pcl6Path[strlen(pcl6Path) - 3] = 0;										// Replace .asc by .pdf
			strcat(pcl6Path, "pdf");
			if (!_access(pcl6Path, 4))												// If generated PDF file found/readable
				ShellExecute(NULL, "open", pcl6Path, NULL, NULL, SW_SHOWNORMAL);	// Open/show it
		}
	}
	return;
}

char* device_PRT::generateRandomString(char *s, const int len) {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	for (int i = 0; i < len; ++i) {
		unsigned int n;
		if (rand_s(&n) == 0) {
			s[i] = alphanum[n % (sizeof(alphanum) - 1)];
		}
		else {
			s[i] = 0;
		}
	}

	s[len] = 0;

	return s;
}

char* device_PRT::getTempFileName() {

	DWORD dwRetVal = 0;
	UINT uRetVal = 0;

	char* szTempFileName = (char*)malloc(MAX_PATH);
	TCHAR lpTempPathBuffer[MAX_PATH];

	//  Gets the temp path env string (no guarantee it's a valid path).
	dwRetVal = GetTempPath(MAX_PATH,          // length of the buffer
		lpTempPathBuffer); // buffer for path 
	if (dwRetVal < MAX_PATH - 11 && (dwRetVal != 0))
	{
		strcat(lpTempPathBuffer, "vDos\\");
		if (!CreateDirectory(lpTempPathBuffer, NULL)) {
			DWORD lastError = GetLastError();
			if (lastError != ERROR_ALREADY_EXISTS) {
				ErrorDialog("Could not create temp folder '%s'", lpTempPathBuffer);
				return NULL;
			}
		}
		char prefix[4];
		//  Generates a temporary file name. 
		uRetVal = GetTempFileName(lpTempPathBuffer, // directory for tmp files
			generateRandomString(prefix, 3),     // temp file name prefix 
			0,                // create unique name 
			szTempFileName);  // buffer for name 
		if (uRetVal != 0)
		{
			return szTempFileName;
		}
	}
	return NULL;
}

void device_PRT::executeCmd(char * pathName, char* filename, BOOL wait) {

	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	char* cmdline = (char*)malloc(MAX_PATH);

	sprintf_s(cmdline, MAX_PATH, "%s \"%s\"", pathName, filename);

	if (!CreateProcess(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {	// Start cmdline
		ErrorDialog("Could not execute '%s'", pathName);
	}
	if (wait) {
		WaitForSingleObject(pi.hProcess, INFINITE);
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return;
}

void device_PRT::ErrorDialog(const char *msg, ...) {

	char* buffer;
	int len, rv;

	va_list args;
	va_start(args, msg);
	len = _vscprintf(msg, args) + 1; // _vscprintf doesn't count terminating '\0'
	buffer = (char*)malloc(len * sizeof(char));
	rv = vsprintf_s(buffer, len, msg, args);
	va_end(args);

	if (rv > 0) {
		MessageBeep(MB_ICONERROR);
		MessageBox(sdlHwnd, buffer, "vDos - Error", MB_OK | MB_ICONERROR);
	}
	free(buffer);
}

void device_PRT::CommitData()
{
	timeOutAt = 0;

	BOOL wait = 0;

	if (destination == "direct") {

		DWORD dwBytesWritten = 0;

		HANDLE lptPort = CreateFile(name, (GENERIC_READ | GENERIC_WRITE), 0, 0, OPEN_EXISTING, 0, 0);

		BOOL bErrorFlag = WriteFile(
			lptPort,           // open file handle
			rawdata.c_str(),      // start of data to write
			(DWORD)rawdata.size(),  // number of bytes to write
			&dwBytesWritten, // number of bytes that were written
			NULL);            // no overlapped structure

		if (FALSE == bErrorFlag)
		{
			ErrorDialog("ERROR: Unable open '%s'", name);
		}
		rawdata.clear();
		CloseHandle(lptPort);
		return;
	}

	if (!destination.empty()) {
		char* tmpFile = getTempFileName();

		if (tmpFile == NULL) {
			ErrorDialog("ERROR: Unable to get temporary filename.");
			return;
		}

		FILE* fh = fopen(tmpFile, "wb");

		if (fh) {
			if (fwrite(rawdata.c_str(), rawdata.size(), 1, fh)) {
				fclose(fh);
				executeCmd((char*)destination.c_str(), tmpFile, wait);
				if (wait) {
					remove(tmpFile);
				}
			} else {
				ErrorDialog("ERROR: Unable to write to temporary file '%s'.", tmpFile);
			}
		} else {
			ErrorDialog("ERROR: Unable to create temporary file '%s'.", tmpFile);
		}
		free(tmpFile);
	} else {
		ErrorDialog("ERROR: Port '%s' is not configured.", name);
	}
	rawdata.clear();
}

Bit16u device_PRT::GetInformation(void)
{
	return 0x80A0;
}

static char* PD_select[] = { "/SEL", "/PDF", "/RTF" };
static char DP_lCode[] = "  ";

device_PRT::device_PRT(const char *pname, const char* cmd)
{
	// pname: LPT1-LPT9, COM1-COM9
	// cmd:
	//  1.  Not set or empty								: DOSPrinter is assumed with "/SEL /LINES /CPIA /LEFT0.50 /TOP0.50 /Lngxx" switches.
	//														  If the data is recognized being PCL or Postscript, (ps)pcl6 is started.
	//	2.	/SEL, /PDF or /RTF...							: DOSPrinter is called with these switches (and /Lngxx if not included).
	//  3.	clipboard										: Data is put on the clipboard.
	//	4.	dummy											: Data is discarded, output in #LPT1-9/#COM1-9 is in ASCII.
	//	5.	<Windows command/program> [options]				: Fallthru, cCommand/program [options] is started.

	//char	ptmp[5];

	//strcpy(ptmp, pname);
	//upcase(ptmp);
	
	SetName(pname);

	if (_strcmpi(cmd, "direct") == 0) {
		destination = "direct";
		return;
	}

	if (wpVersion && pname[3] == '9')									// LPT9/COM9 in combination with WP
	{
		destination = "clipboard";
		fastCommit = true;
	} else {
		destination = cmd;
		fastCommit = false;
	}
}

device_PRT::~device_PRT()
{
}
