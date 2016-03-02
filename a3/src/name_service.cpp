#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include <algorithm>
#include <arpa/inet.h> // network bytes order
#include <cassert>
#include <exception>
#include <iostream>
#include <iterator>

// utility methods
// logs
static NameService::LogEntry pop_entry(std::stringstream &ss);
static void push(std::stringstream &ss, const NameService::LogEntry &entry);

void NameService::cleanup_disconnected_ids(NameIds &ids)
{
	// clean up disconnected nodes to avoid making bad decisions
	NameIds::iterator it = ids.begin();

	while(it != ids.end())
	{
		if(id_to_name.find(*it) == id_to_name.end())
		{
			// not found in the id_to_name mapping -- absolutely disconnected
			NameIds::iterator it_temp = it;
			++it;
			ids.erase(it_temp);
		}
		else
		{
			// this node is still perceived as connected (accurate up to the logs)
			++it;
		}
	}
}

int NameService::get_listen_port(unsigned id, int &ret) const
{
	ListenPortMap::const_iterator it = this->id_to_listen_ports.find(id);

	if(it == this->id_to_listen_ports.end())
	{
		// not a server
		return -1;
	}

	ret = it->second;
	return 0;
}

void NameService::add_listen_port(unsigned id, int port)
{
#ifndef NDEBUG
	std::cout << "registering listen port " << port << " for node id:" << id << std::endl;
#endif
	std::pair<ListenPortMap::iterator, bool> ret = this->id_to_listen_ports.insert(std::make_pair(id, port));
	(void) ret;
	// must not have inserted port with the same id previously
	assert(ret.second);
	// add a log entry
	std::stringstream ss;
	push_i32(ss, id);
	push_i32(ss, port);
	LogEntry log = { LISTEN_PORT, ss.str() };
	this->logs.push_back(log);
}

int NameService::suggest(const Function &func, unsigned &ret)
{
	NameIdsWithPivot &pair = this->func_to_ids[func];
	NameIds &ids = pair.first;
	cleanup_disconnected_ids(ids);

	if(ids.size() == 0)
	{
		// no suggestion
		return -1;
	}

	// update the pivot
	pair.second = (pair.second + 1) % ids.size();
	// get the k-th element (pivot)
	NameIds::iterator it = ids.begin();
	std::advance(it, pair.second);
	// pivot is between [0,ids.size()]; therefore it must point to something
	assert(it != ids.end());
	ret = *it;
	return 0;
}

void NameService::register_fn(unsigned id, const Function &func)
{
#ifndef NDEBUG
	std::cout << "registering function for node id:" << id << std::endl;
#endif
	// insert the entry
	Function signiture = func.to_signiture();
	NameIds &ids = this->func_to_ids[signiture].first;
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
	unsigned char name_size = func.name.size();
	std::string ret;
	ss << name_size;
	push_i32(ss, func.types.size());
	ss << func.name;

	for(size_t i = 0; i < func.types.size(); i++)
	{
		push_i32(ss, func.types[i]);
	}
}

Function func_from_sstream(std::stringstream &ss)
{
	unsigned char size;
	ss >> size;
	unsigned num_args = pop_i32(ss);
	Function func;
	func.name = raw_read(ss, size);

	for(size_t i = 0; i < num_args; i++)
	{
		func.types.push_back(pop_i32(ss));
	}

	return func;
}

int NameService::resolve(unsigned id, Name &ret) const
{
	RightMap::const_iterator it = this->id_to_name.find(id);

	if(it == this->id_to_name.end())
	{
		return -1;
	}

	ret = it->second;
	return 0;
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

void NameService::register_name(unsigned id, const Name &name)
{
#ifndef NDEBUG
	std::cout << "Registering name id:" << id << " ip:" << to_ipv4_string(name.ip) << "(" << (unsigned)name.ip << ") port:" << name.port << std::endl;
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

void NameService::remove_name(unsigned id)
{
	RightMap::iterator it = this->id_to_name.find(id);
	// this is an internal method which is called to remove an EXISTING name
	assert(it != this->id_to_name.end());
	this->name_to_id.erase(it->second);
	this->id_to_name.erase(id);
	this->id_to_listen_ports.erase(id);
}

void NameService::kill(unsigned id)
{
#ifndef NDEBUG
	std::cout << "killing node:" << id << std::endl;
#endif
	this->remove_name(id);
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

// needed for std::map
bool operator< (const Function& lhs, const Function& rhs)
{
	Function lhs_sig = lhs.to_signiture();
	Function rhs_sig = rhs.to_signiture();
	return std::make_pair(lhs_sig.name, lhs_sig.types)
	       < std::make_pair(rhs_sig.name, rhs_sig.types);
}

// needed for std::map
bool operator< (const Name& lhs, const Name& rhs)
{
	return std::make_pair(lhs.ip, lhs.port) < std::make_pair(rhs.ip, rhs.port);
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

size_t get_arg_car(int arg_type)
{
	// truncate to lower 16 bits
	return static_cast<unsigned short>(arg_type);
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
	push_i32(ss, num_delta);

	for(long i = 0; i < num_delta; i++)
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
	unsigned num_delta = pop_i32(ss);
	LogEntries entries;
	std::cout << "number of logs:" << num_delta << std::endl;

	for(size_t i = 0; i < num_delta; i++)
	{
		unsigned log_version = pop_i32(ss);
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
				unsigned id = pop_i32(entry_ss);
				Name name;
				name.ip = pop_i32(entry_ss);
				name.port = pop_i32(entry_ss);
				this->register_name(id, name);
			}
			break;

			case KILL_NODE:
			{
				unsigned id = pop_i32(entry_ss);
				this->kill(id);
			}
			break;

			case NEW_FUNC:
			{
				unsigned id = pop_i32(entry_ss);
				Function func = func_from_sstream(entry_ss);
				this->register_fn(id, func);
			}
			break;

			case LISTEN_PORT:
			{
				unsigned id = pop_i32(entry_ss);
				int port = pop_i32(entry_ss);
				this->add_listen_port(id, port);
				break;
			}

			default:
				//unreachable
				assert(false);
				break;
		}
	}

	return -1;
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
