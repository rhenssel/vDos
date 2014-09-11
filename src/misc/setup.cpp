#include "vDos.h"
#include "setup.h"
#include "control.h"
#include "support.h"
#include <fstream>
#include <string>
#include <sstream>
#include <list>
#include <stdlib.h>
#include <stdio.h>
#include <limits>
#include <limits.h>

using namespace std;

void Value::destroy() throw()
	{
	if (type == V_STRING)
		delete _string;
	}

Value& Value::copy(Value const& in) throw(WrongType)
	{
	if (this != &in)
		{					// Selfassigment!
		if (type != V_NONE && type != in.type)
			throw WrongType();
		destroy();
		plaincopy(in);
		}
	return *this;
	}

void Value::plaincopy(Value const& in) throw()
	{
	type = in.type;
	_int = in._int;
	_bool = in._bool;
	if (type == V_STRING)
		_string = new string(*in._string);
	}

Value::operator bool () const throw(WrongType)
	{
	if (type != V_BOOL)
		throw WrongType();
	return _bool;
	}

Value::operator int () const throw(WrongType)
	{
	if (type != V_INT)
		throw WrongType();
	return _int;
	}

Value::operator char const* () const throw(WrongType)
	{
	if (type != V_STRING)
		throw WrongType();
	return _string->c_str();
	}

bool Value::operator==(Value const& other)
	{
	if (this == &other)
		return true;
	if (type != other.type)
		return false;
	switch(type)
		{
	case V_BOOL: 
		if (_bool == other._bool)
			return true;
		break;
	case V_INT:
		if (_int == other._int)
			return true;
		break;
	case V_STRING:
		if ((*_string) == (*other._string))
			return true;
		break;
	default:
		E_Exit("SETUP: Comparing stuff that doesn't make sense");
		break;
		}
	return false;
	}

bool Value::SetValue(string const& in, Etype _type) throw(WrongType)
	{
	/* Throw exception if the current type isn't the wanted type 
	 * Unless the wanted type is current.
	 */
	if (_type == V_CURRENT && type == V_NONE)
		throw WrongType();
	if (_type != V_CURRENT)
		{ 
		if (type != V_NONE && type != _type)
			throw WrongType();
		type = _type;
		}
	bool retval = true;
	switch(type)
		{
	case V_INT:
		retval = set_int(in);
		break;
	case V_BOOL:
		retval = set_bool(in);
		break;
	case V_STRING:
		set_string(in);
		break;
	case V_NONE:
	case V_CURRENT:
	default:
		throw WrongType();				// Shouldn't happen!/Unhandled
		break;
		}
	return retval;
	}

bool Value::set_int(string const &in)
	{
	istringstream input(in);
	Bits result = INT_MIN;
	input >> result;
	if (result == INT_MIN)
		return false;
	_int = result;
	return true;
	}

bool Value::set_bool(string const &in)
	{
	istringstream input(in);
	string result;
	input >> result;
	lowcase(result);
	if (result == "true" || result == "on")
		_bool = true;
	else if (!(result == "false" || result == "off"))
		return false;
	return true;
	}

void Value::set_string(string const & in)
	{
	if (!_string)
		_string = new string();
	_string->assign(in);
	}

bool Prop_int::SetValue(std::string const& input)
	{
	Value val;
	if (!val.SetValue(input, Value::V_INT))
		return false;
	return SetVal(val);
	}

bool Prop_string::SetValue(std::string const& input)
	{
	// Special version for lowcase stuff
	std::string temp(input);
	lowcase(temp);
	Value val(temp, Value::V_STRING);
	return SetVal(val);
	}
	
bool Prop_bool::SetValue(std::string const& input)
	{
	return value.SetValue(input, Value::V_BOOL);
	}

void Section_prop::Add_int(string const& _propname, int _value)
	{
	Prop_int* test = new Prop_int(_propname, _value);
	properties.push_back(test);
	}

void Section_prop::Add_string(string const& _propname, char const * const _value)
	{
	Prop_string* test = new Prop_string(_propname, _value);
	properties.push_back(test);
	}

void Section_prop::Add_bool(string const& _propname, bool _value)
	{
	Prop_bool* test = new Prop_bool(_propname, _value);
	properties.push_back(test);
	}

int Section_prop::Get_int(string const&_propname) const
	{
	for (const_it tel = properties.begin(); tel != properties.end(); tel++)
		if ((*tel)->propname == _propname)
			return ((*tel)->GetValue());
	return 0;
	}

bool Section_prop::Get_bool(string const& _propname) const
	{
	for (const_it tel = properties.begin(); tel != properties.end(); tel++)
		if ((*tel)->propname == _propname)
			return ((*tel)->GetValue());
	return false;
	}

const char* Section_prop::Get_string(string const& _propname) const
	{
	for (const_it tel = properties.begin(); tel != properties.end(); tel++)
		if ((*tel)->propname == _propname)
			return ((*tel)->GetValue());
	return "";
	}

void trim(string& in)
	{
	string::size_type loc = in.find_first_not_of(" \r\t\f\n");
	if (loc != string::npos)
		in.erase(0, loc);
	loc = in.find_last_not_of(" \r\t\f\n");
	if (loc != string::npos)
		in.erase(loc+1);
	}

bool Section_prop::HandleInputline(string const& gegevens)
	{
	string::size_type loc = gegevens.find('=');
	if (loc == string::npos)
		return false;
	string name = gegevens.substr(0, loc);
	string val = gegevens.substr(loc + 1);
	// trim the results incase there were spaces somewhere
	trim(name);
	trim(val);
	for (it tel = properties.begin(); tel != properties.end(); tel++)
		if (!stricmp((*tel)->propname.c_str(), name.c_str()))
			return (*tel)->SetValue(val);
	if (name.length() == 4 && (!strnicmp(name.c_str(), "LPT", 3) || !strnicmp(name.c_str(), "COM", 3)) && (name.substr(3,1) > "0" && name.substr(3,1) <= "9"))
		{
		properties.push_back(new Prop_string(name.c_str(), val.c_str()));
		return true;
		}
	return false;
	}

Section_prop* Config::SetSection_prop(void (*_initfunction)(Section*))
	{
	Section_prop* blah = new Section_prop();
	blah->AddInitFunction(_initfunction);
	oneSection = blah;
	return blah;
	}

Section_prop::~Section_prop()
	{
	// ExecuteDestroy should be here else the destroy functions use destroyed properties
	ExecuteDestroy();
	// Delete properties themself (properties stores the pointer of a prop
	for (it prop = properties.begin(); prop != properties.end(); prop++)
		delete (*prop);
	}

void Config::Init()
	{
	oneSection->ExecuteInit();
	}

void Section::AddInitFunction(SectionFunction func)
	{
	initfunctions.push_back(Function_wrapper(func));
	}

void Section::AddDestroyFunction(SectionFunction func)
	{
	destroyfunctions.push_front(Function_wrapper(func));
	}

void Section::ExecuteInit()
	{
	int i = 0;
	typedef std::list<Function_wrapper>::iterator func_it;
	for (func_it tel = initfunctions.begin(); tel != initfunctions.end(); tel++)
		(*tel).function(this);
	}

void Section::ExecuteDestroy()
	{
	typedef std::list<Function_wrapper>::iterator func_it;
	for (func_it tel = destroyfunctions.begin(); tel != destroyfunctions.end(); )
		{
		(*tel).function(this);
		tel = destroyfunctions.erase(tel);	// Remove destroyfunction once used
		}
	}

Config::~Config()
	{
	delete oneSection;
	}

Section* Config::GetSection() const
	{
	return oneSection;
	}

void Config::ParseConfigFile()
	{
	std::string errors;
	ifstream in("config.txt");
	if (!in)
		return;

	string gegevens;
	Section * currentsection = GetSection();
	while (getline(in, gegevens))
		{
		trim(gegevens);				// strip leading/trailing whitespace
		if (gegevens.size() && strnicmp(gegevens.c_str(), "rem", 3))
			if (!currentsection->HandleInputline(gegevens))
				{
				if (errors.empty())
					errors = "CONFIG.TXT - unresolved lines:\n";
				if (gegevens.length() > 40)
					errors += "\n" + gegevens.substr(0, 37) + "...";
				else
 					errors += "\n" + gegevens;
//				LOG_MSG("CONFIG.TXT unresolved: %s", gegevens.c_str());
				}
		}
	if (!errors.empty())
		MessageBox(NULL, errors.c_str(), "vDos - Warning", MB_OK|MB_ICONWARNING);
	}

void Config::SetStartUp(void (*_function)(void))
	{
	_start_function = _function;
	}

void Config::StartUp(void)
	{
	(*_start_function)();
	}

bool CommandLine::FindString(char const * const name, std::string & value, bool remove)
	{
	cmd_it it, it_next;
	if (!(FindEntry(name, it, true)))
		return false;
	it_next = it;
	it_next++;
	value = *it_next;
	if (remove)
		cmds.erase(it, ++it_next);
	return true;
	}

bool CommandLine::FindCommand(unsigned int which, std::string & value)
	{
	if (which < 1 || which > cmds.size())
		return false;
	cmd_it it = cmds.begin();
	for (; which > 1; which--)
		it++;
	value = (*it);
	return true;
	}

bool CommandLine::FindEntry(char const * const name, cmd_it & it, bool neednext)
	{
	for (it = cmds.begin(); it != cmds.end(); it++)
		if (!stricmp((*it).c_str(), name))
			{
			cmd_it itnext = it;
			itnext++;
			if (neednext && (itnext == cmds.end()))
				return false;
			return true;
			}
	return false;
	}

bool CommandLine::FindStringBegin(char const* const begin, std::string & value, bool remove)
	{
	size_t len = strlen(begin);
	for (cmd_it it = cmds.begin(); it != cmds.end(); it++)
		if (strncmp(begin, (*it).c_str(), len) == 0)
			{
			value = ((*it).c_str() + len);
			if (remove)
				cmds.erase(it);
			return true;
			}
	return false;
	}

bool CommandLine::FindStringRemain(char const * const name, std::string & value)
	{
	cmd_it it;
	value = "";
	if (!FindEntry(name, it))
		return false;
	for (it++; it != cmds.end(); it++)
		{
		value += " ";
		value += (*it);
		}
	return true;
	}

unsigned int CommandLine::GetCount(void)
	{
	return (unsigned int)cmds.size();
	}

CommandLine::CommandLine(char const * const name, char const * const cmdline)
	{
	if (name)
		file_name = name;
	// Parse the cmds and put them in the list
	bool inword, inquote;
	char c;
	inword = false;
	inquote = false;
	std::string str;
	const char* c_cmdline = cmdline;
	while ((c = *c_cmdline) != 0)
		{
		if (inquote)
			{
			if (c != '"')
				str += c;
			else
				{
				inquote = false;
				cmds.push_back(str);
				str.erase();
				}
			}
		else if (inword)
			{
			if (c != ' ')
				str += c;
			else
				{
				inword = false;
				cmds.push_back(str);
				str.erase();
				}
			} 
		else if (c == '"')
			inquote = true;
		else if	(c != ' ')
			{
			str += c;
			inword = true;
			}
		c_cmdline++;
		}
	if (inword || inquote)
		cmds.push_back(str);
	}

void CommandLine::Shift(unsigned int amount)
	{
	while (amount--)
		{
		file_name = cmds.size() ? (*(cmds.begin())) : "";
		if (cmds.size())
			cmds.erase(cmds.begin());
		}
	}
