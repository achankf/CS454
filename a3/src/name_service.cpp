#include "debug.hpp"
#include "name_service.hpp"
#include "rpc.h"
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

// utility methods
// logs
static NameService::LogEntry pop_entry(std::stringstream &ss);
static void push(std::stringstream &ss, const NameService::LogEntry &entry);

NameService::NameService()
{
	int retval = pthread_mutex_init(&this->mutex, NULL);
	(void) retval;
	assert(retval == 0);
}

NameService::~NameService()
{
	pthread_mutex_destroy(&this->mutex);
}

int NameService::suggest_helper(Postman &postman, const Function &func, unsigned &ret, bool is_binder)
{
	NameIdsWithPivot &pair = this->func_to_ids[func];
	NameIds &ids = pair.first;
#ifndef NDEBUG
	print_function(func);
#endif

	if(ids.size() == 0)
	{
		// no suggestion
		return NO_AVAILABLE_SERVER;
	}

	// update the pivot
	int next = (pair.second + 1) % ids.size();
	// get the k-th element (pivot)
	NameIds::iterator it = ids.begin();
	std::advance(it, next);
	// pivot is between [0,ids.size()]; therefore it must point to something
	assert(it != ids.end());
	// test if the server is still alive
	Name name;
	int id = *it;

	// if a function is registered under an id, the id must have existed...
	if(this->resolve_helper(id, name) < 0)
	{
		// this happens when the name entry is cleaned up by other suggest() call for a different function, which cleaned up zombie name entries
		// remove this candadate
		ids.erase(id);
		// recurse with one less candidate
		return suggest_helper(postman, func, ret, is_binder);
	}

	{
		ScopedConnection conn(postman, name.ip, name.port);

		if(conn.get_fd() >= 0)
		{
#ifndef NDEBUG
			std::cout << "suggestion for func:" << func.name << " to id:" << id << std::endl;
#endif
			pair.second = next;
			// bingo! this server is still alive
			ret = id;
			return OK;
		}
	}

	// only "kill" the id if application is the binder
	// otherwise the logs become inconsistent
	if(is_binder)
	{
		// this server is not alive
		this->kill_helper(id);
	}

	ids.erase(id);
	// recurse with one less candidate
	return this->suggest_helper(postman, func, ret, is_binder);
}

int NameService::suggest(Postman &postman, const Function &func, unsigned &ret, bool is_binder)
{
	ScopedLock lock(this->mutex);
	return this->suggest_helper(postman, func, ret, is_binder);
}

NameService::Names NameService::get_all_names()
{
	Names names;
	ScopedLock lock(this->mutex);
	LeftMap::const_iterator it = this->name_to_id.begin();

	for(; it != this->name_to_id.end(); it++)
	{
		names.push_back(it->first);
	}

	return names;
}

void NameService::register_fn(unsigned id, const Function &func)
{
	ScopedLock lock(this->mutex);
	this->register_fn_helper(id, func);
}

void NameService::register_fn_helper(unsigned id, const Function &func)
{
#ifndef NDEBUG
	std::cout << "registering function for node id:" << id << " log#:" << (this->logs.size() + 1) << std::endl;
	print_function(func);
#endif
	// insert the entry
	NameIds &ids = this->func_to_ids[func].first;
	ids.insert(id);
	// add a log entry
	std::stringstream ss;
	push_i32(ss, id);
	push(ss, func);
	LogEntry entry = {NEW_FUNC, ss.str()};
	this->logs.push_back(entry);
}

void push(std::stringstream &ss, const Function &func)
{
	assert(func.name.size() <= MAX_FUNC_NAME_LEN);
	push_i32(ss, func.name.size());
	push_i32(ss, func.types.size());
	ss << func.name;

	for(size_t i = 0; i < func.types.size(); i++)
	{
		push_i32(ss, func.types[i]);
	}
}

Function func_from_sstream(std::stringstream &ss)
{
	unsigned name_size = pop_i32(ss);
	unsigned num_args = pop_i32(ss);
	Function func;
	func.name = raw_read(ss, name_size);

	for(size_t i = 0; i < num_args; i++)
	{
		func.types.push_back(pop_i32(ss));
	}

	return func;
}

int NameService::resolve(unsigned id, Name &ret)
{
	ScopedLock lock(this->mutex);
	return this->resolve_helper(id, ret);
}

int NameService::resolve_helper(unsigned id, Name &ret) const
{
	RightMap::const_iterator it = this->id_to_name.find(id);

	if(it == this->id_to_name.end())
	{
		return NO_AVAILABLE_SERVER;
	}

	ret = it->second;
	return OK;
}

int NameService::resolve(const Name &name, unsigned &ret)
{
	ScopedLock lock(this->mutex);
	return this->resolve_helper(name, ret);
}

int NameService::resolve_helper(const Name &name, unsigned &ret) const
{
	LeftMap::const_iterator it = this->name_to_id.find(name);

	if(it == this->name_to_id.end())
	{
		return NO_AVAILABLE_SERVER;
	}

	ret = it->second;
	return OK;
}

void NameService::register_name(unsigned id, const Name &name)
{
	ScopedLock lock(this->mutex);
	this->register_name_helper(id, name);
}

void NameService::register_name_helper(unsigned id, const Name &name)
{
#ifndef NDEBUG
	std::cout << "Registering name id:" << id << ' ' << to_format(name) << " log#:" << (this->logs.size()+1) <<std::endl;
#endif
	// this is an internal method which is called to insert a NEW name
	this->id_to_name.insert(std::make_pair(id, name));
	assert(this->name_to_id.find(name) == this->name_to_id.end());
	this->name_to_id.insert(std::make_pair(name,id));
	// add a new log entry
	std::stringstream ss;
	push_i32(ss, id);
	push_i32(ss, name.ip);
	push_i32(ss, name.port);
	LogEntry log = { NEW_NODE, ss.str() };
	this->logs.push_back(log);
}

void NameService::kill(unsigned id)
{
	ScopedLock lock(this->mutex);
	this->kill_helper(id);
}

void NameService::kill_helper(unsigned id)
{
#ifndef NDEBUG
	std::cout << "killing node:" << id << " log#:" << (this->logs.size() + 1) << std::endl;
#endif
	// remove entry from the bi-directional map
	RightMap::iterator it = this->id_to_name.find(id);
	assert(it != this->id_to_name.end());
	this->name_to_id.erase(it->second);
	this->id_to_name.erase(id);
	// add a log entry
	std::stringstream buf;
	push_i32(buf, id);
	LogEntry entry = { KILL_NODE, buf.str() };
	logs.push_back(entry);
}

Function Function::to_signiture() const
{
	Function func;
	func.name = this->name;

	for(size_t i = 0; i < this->types.size(); i++)
	{
		short upper = this->types[i] >> 16;
		short lower = this->types[i];
		lower = lower == 0 ? 0 : 1;
		func.types.push_back((upper << 16) + lower);
	}

	return func;
}

bool operator< (const Function& lhs, const Function& rhs)
{
	Function lhs_sig = lhs.to_signiture();
	Function rhs_sig = rhs.to_signiture();
	return std::make_pair(lhs_sig.name, lhs_sig.types)
	       < std::make_pair(rhs_sig.name, rhs_sig.types);
}

bool operator< (const Name& lhs, const Name& rhs)
{
	return std::make_pair(lhs.ip, lhs.port) < std::make_pair(rhs.ip, rhs.port);
}

Function to_function(const char *name_cstr, int *argTypes)
{
	assert(name_cstr != NULL);
	assert(argTypes != NULL);
	std::string name(name_cstr);

	if(name.size() > MAX_FUNC_NAME_LEN)
	{
		// should not happen since the assignment guarantees this
		assert(false);
		throw std::exception();
	}

	std::vector<int> argTypesVec;

	while(*argTypes != 0)
	{
		argTypesVec.push_back(*argTypes);
		argTypes++;
	}

	Function func = {name, argTypesVec};
	return func;
}

unsigned NameService::get_version()
{
	ScopedLock lock(this->mutex);
	return this->get_version_helper();
}

unsigned NameService::get_version_helper() const
{
	return this->logs.size();
}

bool is_arg_input(int arg_type)
{
	return arg_type & (1 << ARG_INPUT);
}

bool is_arg_output(int arg_type)
{
	return arg_type & (1 << ARG_OUTPUT);
}

size_t get_arg_car(int arg_type)
{
	// truncate to lower 16 bits
	unsigned short low = static_cast<unsigned short>(arg_type);
	return std::max(static_cast<unsigned short>(1), low);
}

bool is_arg_scalar(int arg_type)
{
	return static_cast<unsigned short>(arg_type) == 0;
}

int get_arg_data_type(int arg_type)
{
	// take the second byte
	return (arg_type >> 16) & 0xff;
}

std::string NameService::get_logs(unsigned since)
{
	std::stringstream ss;
	ScopedLock lock(this->mutex);
	unsigned num_delta = (this->get_version_helper() <= since)
	                     ? 0
	                     : this->get_version_helper() - since;
#ifndef NDEBUG
	std::cout << "sending log (num_delta:" << num_delta << ") since:" << since << " my version:" << get_version_helper() << std::endl;
#endif
	push_i32(ss, num_delta);

	for(unsigned i = 0; i < num_delta; i++)
	{
		unsigned cur_ver= since + i;
		// note: vector starts from zero but logs[0] contains version 1
		push_i32(ss, cur_ver + 1);
		push(ss, this->logs[cur_ver]);
	}

	return ss.str();
}

int NameService::apply_logs(std::stringstream &ss)
{
	ScopedLock lock(this->mutex);
	unsigned num_delta = pop_i32(ss);
	LogEntries entries;
#ifndef NDEBUG
	std::cout << "applying " << num_delta << " logs, current version:" << get_version_helper() << std::endl;
#endif

	for(size_t i = 0; i < num_delta; i++)
	{
		unsigned log_version = pop_i32(ss);
#ifndef NDEBUG
		std::cout << "\tgot log - their version:" << log_version << ", my version:" << get_version_helper() << std::endl;
#endif
		LogEntry entry = pop_entry(ss);
		std::stringstream entry_ss(entry.details);

		if(log_version <= get_version_helper())
		{
			continue;
		}

		assert(log_version == get_version_helper() + 1);

		switch(entry.type)
		{
			case NEW_NODE:
			{
				unsigned id = pop_i32(entry_ss);
				Name name;
				name.ip = pop_i32(entry_ss);
				name.port = pop_i32(entry_ss);
				this->register_name_helper(id, name);
			}
			break;

			case KILL_NODE:
			{
				unsigned id = pop_i32(entry_ss);
				this->kill_helper(id);
			}
			break;

			case NEW_FUNC:
			{
				unsigned id = pop_i32(entry_ss);
				Function func = func_from_sstream(entry_ss);
				this->register_fn_helper(id, func);
			}
			break;
		}
	}

#ifndef NDEBUG
	std::cout << "finished apply logs, current version:" << get_version_helper() << std::endl;
#endif
	return OK;
}

static void push(std::stringstream &ss, const NameService::LogEntry &entry)
{
	push_i32(ss, entry.type);
	push(ss, entry.details);
}

static NameService::LogEntry pop_entry(std::stringstream &ss)
{
	typedef NameService::LogEntry LogEntry;
	LogEntry entry;
	entry.type = static_cast<NameService::LogType>(pop_i32(ss));
	entry.details = pop_string(ss);
	return entry;
}
