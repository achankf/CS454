#ifndef _name_service_hpp_
#define _name_service_hpp_

#include "sockets.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

#define MAX_FUNC_NAME_LEN 64

struct Name
{
	int ip;
	int port;
};

typedef std::set<int> NameIds;

struct Function
{
public: //data
	std::string name;
	std::vector<int> types;
public: // methods
	Function to_signiture() const;
};

class NameService
{
public: // typedefs
	// the following should simulate bidirectional search
	typedef std::map<Name,unsigned int> LeftMap;
	typedef std::map<unsigned int,Name> RightMap;

	enum LogType
	{
		NEW_NODE,
		KILL_NODE,
		NEW_FUNC
	};

	struct LogEntry
	{
		LogType type;
		std::string detail;
	};
	typedef std::vector<LogEntry> LogEntries;

private: // data
	LeftMap name_to_id;
	RightMap id_to_name;
	std::map<Function, std::set<int> > func_to_ids;
	LogEntries logs;

private: // methods

	// internal methods to manipulate the bidirectional mapping of name<->id
	int insert_name(const Name &name);
	void remove_name(unsigned int id);

public: // methods
	const NameIds &suggest(const Function &func);
	int resolve(const Name &name, unsigned int &ret);
	unsigned int get_version() const;

	// non-binder should update NameService using apply_logs
	void apply_logs(LogEntries &delta);

	// these 3 methods affect the logs
	// they should be called directly ONLY by the binder
	void kill(unsigned int id);
	void register_fn(const Function &func, const Name &name);
	unsigned int register_name(unsigned int id, const Name &name);
};

// utility methods
Function to_function(const char *name_cstr, int *argTypes);
std::string to_string(Function &func);
Function func_from_string(std::string str);
std::string format_arg(int arg_type);
void print_function(Function &func);
bool is_arg_input(int arg_type);
bool is_arg_output(int arg_type);
bool is_arg_array(int arg_type, size_t &size);
int get_arg_data_type(int arg_type);

#endif
