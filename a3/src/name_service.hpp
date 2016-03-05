#ifndef _name_service_hpp_
#define _name_service_hpp_

#include "rpc.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <pthread.h>

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
		NEW_FUNC
	};

	struct LogEntry
	{
		LogType type;
		std::string details;
	};
	typedef std::vector<LogEntry> LogEntries;

	typedef std::set<unsigned> NameIds;
	typedef std::vector<Name> Names;
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
	pthread_mutex_t mutex;

private: // methods

	// these are unsynchronized version of the public methods
	int resolve_helper(const Name &name, unsigned &ret) const;
	int resolve_helper(unsigned id, Name &ret) const;
	int suggest_helper(Postman &postman, const Function &func, unsigned &ret);
	unsigned get_version_helper() const;
	void kill_helper(unsigned id);
	void register_fn_helper(unsigned id, const Function &func);
	void register_name_helper(unsigned id, const Name &name);

public: // methods
	NameService();
	~NameService();

	Names get_all_names();
	int resolve(const Name &name, unsigned &ret);
	int resolve(unsigned id, Name &ret);
	int suggest(Postman &postman, const Function &func, unsigned &ret);
	unsigned get_version();

	// non-binder should update NameService using apply_logs
	int apply_logs(std::stringstream &ss);
	std::string get_logs(unsigned since);

	// these 3 methods affect the logs
	// they should be called directly ONLY by the binder
	void kill(unsigned id);
	void register_fn(unsigned id, const Function &func);
	void register_name(unsigned id, const Name &name);
};

// utility methods

// functions
Function func_from_sstream(std::stringstream &ss);
Function to_function(const char *name_cstr, int *argTypes);
void push(std::stringstream &ss, const Function &func);

// arg types
bool is_arg_input(int arg_type);
bool is_arg_output(int arg_type);
bool is_arg_scalar(int arg_type);
int get_arg_data_type(int arg_type);
size_t get_arg_car(int arg_type);

// needed for std::map
bool operator< (const Function& lhs, const Function& rhs);
bool operator< (const Name& lhs, const Name& rhs);

#endif
