#ifndef VDOS_CONTROL_H
#define VDOS_CONTROL_H

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#pragma warning ( disable : 4290 )
#endif

#ifndef vDOS_PROGRAMS_H
#include "programs.h"
#endif
#ifndef vDOS_SETUP_H
#include "setup.h"
#endif

#ifndef CH_LIST
#define CH_LIST
#include <list>
#endif

#ifndef CH_VECTOR
#define CH_VECTOR
#include <vector>
#endif

#ifndef CH_STRING
#define CH_STRING
#include <string>
#endif

class Config
	{
private:
	Section* oneSection;
	void (* _start_function)(void);
public:
	~Config();

	Section_prop * SetSection_prop(void (*_initfunction)(Section*));
	
	Section* GetSection() const;

	void SetStartUp(void (*_function)(void));
	void Init();
	void StartUp();
	void ParseConfigFile();
	};

#endif
