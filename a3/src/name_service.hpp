#ifndef _name_service_hpp_
#define _name_service_hpp_

#include "rpc.h"
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
	typedef std::map<Name,unsigned> LeftMap;
	typedef std::map<unsigned,Name> RightMap;

	enum LogType
	{
		NOT_USED,
		NEW_NODE,
		KILL_NODE,
		NEW_FUNC
	};

	struct LogEntry
	{
		LogType type;
		std::string details;
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
	void remove_name(unsigned id);

public: // methods
	const NameIds &suggest(const Function &func);
	int resolve(const Name &name, unsigned &ret) const;
	unsigned get_version() const;

	// non-binder should update NameService using apply_logs
	std::string get_logs(unsigned since) const;
	int apply_logs(const std::string &partial_logs);

	// these 3 methods affect the logs
	// they should be called directly ONLY by the binder
	void kill(unsigned id);
	void register_fn(const Function &func, const Name &name);
	unsigned register_name(unsigned id, const Name &name);
};

// utility methods

// functions
Function to_function(const char *name_cstr, int *argTypes);
std::string to_net_string(const Function &func);
Function func_from_string(const std::string &str);

// arg types
bool is_arg_input(int arg_type);
bool is_arg_output(int arg_type);
bool is_arg_array(int arg_type, size_t &size);
int get_arg_data_type(int arg_type);

// logs
std::string to_net_string(const NameService::LogEntry &entry);

#endif
