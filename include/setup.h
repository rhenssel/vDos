#ifndef vDOS_SETUP_H
#define vDOS_SETUP_H

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#pragma warning ( disable : 4290 )
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

#ifndef CH_CSTDIO
#define CH_CSTDIO
#include <cstdio>
#endif

class Value
	{
/* 
 * Multitype storage container that is aware of the currently stored type in it.
 * Value st = "hello";
 * Value in = 1;
 * st = 12			//Exception
 * in = 12			//works
 */
private:
	bool _bool;
	int _int;
	std::string* _string;
public:
	class WrongType { };	// Conversion error class
	enum Etype { V_NONE, V_BOOL, V_INT, V_STRING, V_CURRENT} type;
	
	// Constructors
	Value()                      :_string(0),   type(V_NONE)                  { };
	Value(int in)                :_int(in),     type(V_INT)                   { };
	Value(bool in)               :_bool(in),    type(V_BOOL)                  { };
	Value(std::string const& in) :_string(new std::string(in)),type(V_STRING) { };
	Value(char const * const in) :_string(new std::string(in)),type(V_STRING) { };
	Value(Value const& in):_string(0) {plaincopy(in);}
	~Value() { destroy();};
	Value(std::string const& in,Etype _t) :_string(0),type(V_NONE) {SetValue(in,_t);}
	
	// Assigment operators
	Value& operator= (int in) throw(WrongType)                { return copy(Value(in));}
	Value& operator= (bool in) throw(WrongType)               { return copy(Value(in));}
	Value& operator= (std::string const& in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (char const * const in) throw(WrongType) { return copy(Value(in));}
	Value& operator= (Value const& in) throw(WrongType)       { return copy(Value(in));}

	bool operator== (Value const & other);
	operator bool () const throw(WrongType);
	operator int () const throw(WrongType);
	operator char const* () const throw(WrongType);
	bool SetValue(std::string const& in,Etype _type = V_CURRENT) throw(WrongType);

private:
	void destroy() throw();
	Value& copy(Value const& in) throw(WrongType);
	void plaincopy(Value const& in) throw();
	bool set_int(std::string const&in);
	bool set_bool(std::string const& in);
	void set_string(std::string const& in);
	};

class Property
	{
public:
	const std::string propname;

	Property(std::string const& _propname):propname(_propname) { }
	virtual	bool SetValue(std::string const& str) = 0;
	Value const& GetValue() const { return value;}
	// Set interval value to in or default if in is invalid. force always sets the value.
	bool SetVal(Value const& in)
		{
		value = in;
		return true;
		}
	virtual ~Property(){ } 
protected:
	Value value;
	};

class Prop_int:public Property
	{
public:
	Prop_int(std::string const& _propname, int _value):Property(_propname)
		{ 
		value = _value;
		}
	bool SetValue(std::string const& in);
	~Prop_int(){ }
	};

class Prop_bool:public Property
	{
public:
	Prop_bool(std::string const& _propname, bool _value):Property(_propname)
		{ 
		value = _value;
		}
	bool SetValue(std::string const& in);
	~Prop_bool(){ }
	};

class Prop_string:public Property
	{
public:
	Prop_string(std::string const& _propname, char const * const _value):Property(_propname)
		{ 
		value = _value;
		}
	bool SetValue(std::string const& in);
	~Prop_string(){ }
	};

class Section
	{
private:
	typedef void (*SectionFunction)(Section*);
	struct Function_wrapper {
		SectionFunction function;
		Function_wrapper(SectionFunction const _fun)
			{
			function=_fun;
			}
		};
	std::list<Function_wrapper> initfunctions;
	std::list<Function_wrapper> destroyfunctions;
public:
	Section() {  }

	void AddInitFunction(SectionFunction func);
	void AddDestroyFunction(SectionFunction func);
	void ExecuteInit();
	void ExecuteDestroy();

	virtual bool HandleInputline(std::string const& _line) = 0;
	virtual ~Section() { /*Children must call executedestroy ! */}
	};

class Section_prop:public Section
	{
private:
	std::list<Property*> properties;
	typedef std::list<Property*>::iterator it;
	typedef std::list<Property*>::const_iterator const_it;

public:
	Section_prop():Section(){}
	void Add_int(std::string const& _propname, int _value = 0);
	void Add_string(std::string const& _propname, char const * const _value = NULL);
	void Add_bool(std::string const& _propname, bool _value = false);

	int Get_int(std::string const& _propname) const;
	const char* Get_string(std::string const& _propname) const;
	bool Get_bool(std::string const& _propname) const;
	bool HandleInputline(std::string const& gegevens);
	// ExecuteDestroy should be here else the destroy functions use destroyed properties
	virtual ~Section_prop();
	};

class Module_base
	{
	// Base for all hardware and software "devices"
protected:
	Section* m_configuration;
public:
	Module_base(Section* configuration){m_configuration = configuration;};
	virtual ~Module_base(){;};				// Destructors are required
	// Returns true if succesful.
	};
#endif
