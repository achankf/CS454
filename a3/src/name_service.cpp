#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include <algorithm>
#include <arpa/inet.h> // network bytes order
#include <cassert>
#include <exception>
#include <iostream>

// utility methods
// logs
std::string to_net_string(const NameService::LogEntry &entry);
NameService::LogEntry pop_entry(std::stringstream &ss);

const NameIds &NameService::suggest(const Function &func)
{
	return this->func_to_ids[func];
}

void NameService::register_fn(const Function &func, const Name &name)
{
	unsigned id;
	int ret = this->resolve(name, id);
	// > 0 because binder don't register any function
	assert(ret >= 0);
	assert(id > 0);
	Function signiture = func.to_signiture();
	this->func_to_ids[signiture].insert(id);
	// add a log entry
	std::stringstream buf;
	push_u32(buf, id);
	buf << to_net_string(signiture);
	LogEntry entry = {NEW_FUNC, buf.str()};
	this->logs.push_back(entry);
}

std::string to_net_string(const Function &func)
{
	assert(func.name.size() <= MAX_FUNC_NAME_LEN);
	unsigned char name_size = func.name.size();
	std::string ret;
	std::stringstream buf;
	buf << name_size;
	push_u32(buf, func.types.size());
	buf << func.name;

	for(size_t i = 0; i < func.types.size(); i++)
	{
		push_u32(buf, func.types[i]);
	}

	return buf.str();
}

Function func_from_string(const std::string &str)
{
	Function func;
	std::stringstream buf(str);
	unsigned char size;
	buf >> size;
	unsigned num_args = pop_u32(buf);
	func.name = raw_read(buf, size);

	for(size_t i = 0; i < num_args; i++)
	{
		func.types.push_back(pop_u32(buf));
	}

	return func;
}

int NameService::resolve(const Name &name, unsigned &ret) const
{
	LeftMap::const_iterator it = this->name_to_id.find(name);

	if(it == this->name_to_id.end())
	{
		return -1;
	}

	ret = it->second;
	return 0;
}

unsigned NameService::register_name(unsigned id, const Name &name)
{
#ifndef NDEBUG
	std::cout << "Registering name id:" << id << " ip:" << to_ipv4_string(name.ip) << " port:" << name.port << std::endl;
#endif
	// this is an internal method which is called to insert a NEW name
	unsigned not_used;
	this->id_to_name.insert(std::make_pair(id, name));
	assert(this->name_to_id.find(name) == this->name_to_id.end());
	this->name_to_id.insert(std::make_pair(name,id));
	// add a new log entry
	std::stringstream ss;
	push_u32(ss, id);
	push_u32(ss, name.ip);
	push_u32(ss, name.port);
	LogEntry log = { NEW_NODE, ss.str() };
	this->logs.push_back(log);
	return id;
}

void NameService::remove_name(unsigned id)
{
	RightMap::iterator it = this->id_to_name.find(id);
	// this is an internal method which is called to remove an EXISTING name
	assert(it != this->id_to_name.end());
	this->name_to_id.erase(it->second);
	this->id_to_name.erase(id);
}

void NameService::kill(unsigned id)
{
#ifndef NDEBUG
	std::cout << "killing node:" << id << std::endl;
#endif
	this->remove_name(id);
	// add a log entry
	std::stringstream buf;
	push_u32(buf, id);
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

// needed for std::map
bool operator< (const Function& lhs, const Function& rhs)
{
	Function lhs_sig = lhs.to_signiture();
	Function rhs_sig = rhs.to_signiture();
	// name can be the same, but arguments (data, scalar vs array) must be different
	return lhs_sig.name <= rhs_sig.name && lhs.types < rhs.types;
}

// needed for std::map
bool operator< (const Name& lhs, const Name& rhs)
{
	// i.e. can be called from the same machine, but the port must be unique
	return lhs.ip <= rhs.ip && lhs.port < rhs.port;
}

Function to_function(const char *name_cstr, int *argTypes)
{
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

unsigned NameService::get_version() const
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

bool is_arg_array(int arg_type, size_t &size)
{
	size = static_cast<unsigned short>(arg_type);
	return size > 0;
}

int get_arg_data_type(int arg_type)
{
	// take the second byte
	return (arg_type >> 16) & 0xff;
}

std::string NameService::get_logs(unsigned since) const
{
	std::stringstream ss;
	unsigned num_delta = (this->get_version() <= since)
	                     ? 0
	                     : this->get_version() - since;
#ifndef NDEBUG
	std::cout << "sending log (num_delta:" << num_delta << ")" << std::endl;
#endif
	push_u32(ss, num_delta);

	for(long i = 0; i < num_delta; i++)
	{
		unsigned cur_ver= since + i;
		// note: vector starts from zero but logs[0] contains version 1
		push_u32(ss, cur_ver + 1);
		std::cout << "log (" << (cur_ver+1)
		          << ")size:" << to_net_string(this->logs[cur_ver]).size() << std::endl;
		ss << to_net_string(this->logs[cur_ver]);
	}

	return ss.str();
}

int NameService::apply_logs(const std::string &partial_logs)
{
	std::cout << partial_logs.size() << std::endl;
	assert(partial_logs.size() > 4); // must have num_delta
	std::stringstream ss(partial_logs);
	unsigned num_delta = pop_u32(ss);
	LogEntries entries;
	std::cout << "number of logs:" << num_delta << std::endl;

	for(size_t i = 0; i < num_delta; i++)
	{
		unsigned log_version = pop_u32(ss);
		std::cout << "got log (version:" << log_version << "), my version:" << get_version() << std::endl;
		LogEntry entry = pop_entry(ss);
		std::stringstream entry_ss(entry.details);

		if(log_version <= get_version())
		{
			continue;
		}

		assert(log_version == get_version() + 1);

		switch(entry.type)
		{
			case NEW_NODE:
			{
				unsigned id = pop_u32(entry_ss);
				Name name;
				name.ip = pop_u32(entry_ss);
				name.port = pop_u32(entry_ss);
				this->register_name(id, name);
			}
			break;

			case KILL_NODE:
			{
				unsigned id = pop_u32(entry_ss);
				this->kill(id);
			}
			break;

			case NEW_FUNC:
				break;

			default:
				//unreachable
				assert(false);
				break;
		}
	}

	return -1;
}

std::string to_net_string(const NameService::LogEntry &entry)
{
	std::stringstream ss;
	push_u32(ss, entry.type);
	push_string(ss, entry.details);
	return ss.str();
}

NameService::LogEntry pop_entry(std::stringstream &ss)
{
	typedef NameService::LogEntry LogEntry;
	LogEntry entry;
	entry.type = static_cast<NameService::LogType>(pop_u32(ss));
	entry.details = pop_string(ss);
	return entry;
}
