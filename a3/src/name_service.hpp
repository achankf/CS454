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
	enum LogType
	{
		NEW_NODE,
		KILL_NODE,
		NEW_FUNC,
		LISTEN_PORT // need to keep track of ports used for listening requests instead of the ones sending
	};

	struct LogEntry
	{
		LogType type;
		std::string details;
	};
	typedef std::vector<LogEntry> LogEntries;

	typedef std::set<unsigned> NameIds;
	// the pivot (unsigned) is used for round-robin suggestions
	typedef std::pair<NameIds, unsigned> NameIdsWithPivot;

	// the following should simulate bidirectional search
	typedef std::map<Name,unsigned> LeftMap; // Name to id
	typedef std::map<unsigned,Name> RightMap; // id to Name
	typedef std::map<unsigned,int> ListenPortMap; // id to port (only servers have entries)
	typedef std::map<Function, NameIdsWithPivot> FuncPivots;
private: // data
	LeftMap name_to_id;
	RightMap id_to_name;
	std::map<Function, NameIdsWithPivot> func_to_ids;
	LogEntries logs;
	ListenPortMap id_to_listen_ports;

private: // methods

	// note: Function-to-id mapping is not cleaned up in kill()
	// when making suggestions, this method must be ran to cleanup
	// id whose node is disconnected
	void cleanup_disconnected_ids(NameIds &ids);

	// internal methods to manipulate the bidirectional mapping of name<->id
	int insert_name(const Name &name);
	void remove_name(unsigned id);

public: // methods
	int get_listen_port(unsigned id, int &ret) const;
	int resolve(const Name &name, unsigned &ret) const;
	int resolve(unsigned id, Name &ret) const;
	int suggest(const Function &func, unsigned &ret);
	unsigned get_version() const;

	// non-binder should update NameService using apply_logs
	std::string get_logs(unsigned since) const;
	int apply_logs(std::stringstream &ss);

	// these 4 methods affect the logs
	// they should be called directly ONLY by the binder
	void kill(unsigned id);
	void register_fn(unsigned id, const Function &func);
	void register_name(unsigned id, const Name &name);
	void add_listen_port(unsigned id, int port);
};

// utility methods

// functions
Function func_from_sstream(std::stringstream &ss);
Function to_function(const char *name_cstr, int *argTypes);
void push(std::stringstream &ss, const Function &func);

// arg types
bool is_arg_input(int arg_type);
bool is_arg_output(int arg_type);
size_t get_arg_car(int arg_type);
int get_arg_data_type(int arg_type);

#endif
