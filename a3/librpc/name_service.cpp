#include "common.hpp"
#include "name_service.hpp"
#include "rpc.h"
#include <algorithm>
#include <arpa/inet.h> // network bytes order
#include <cassert>
#include <exception>
#include <iostream>

const NameIds &NameService::suggest(const Function &func)
{
	return this->func_to_ids[func];
}

void NameService::register_fn(const Function &func, const Name &name)
{
	unsigned int id;
	int ret = this->resolve(name, id);
	// > 0 because binder don't register any function
	assert(ret >= 0);
	assert(id > 0);
	Function signiture = func.to_signiture();
	this->func_to_ids[signiture].insert(id);
	// add a log entry
	std::stringstream buf;
	push_u32(buf, id);
	buf << to_string(signiture);
	LogEntry entry = {NEW_FUNC, buf.str()};
	this->logs.push_back(entry);
}

std::string to_string(Function &func)
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

Function func_from_string(std::string str)
{
	Function func;
	std::stringstream buf(str);
	unsigned char size;
	buf >> size;
	unsigned int num_args = pop_uint32(buf);
	func.name = get_unformatted(buf, size);

	for(size_t i = 0; i < num_args; i++)
	{
		func.types.push_back(pop_uint32(buf));
	}

	return func;
}

int NameService::resolve(const Name &name, unsigned int &ret)
{
	LeftMap::iterator it = this->name_to_id.find(name);

	if(it == this->name_to_id.end())
	{
		return -1;
	}

	ret = it->second;
	return 0;
}

unsigned int NameService::register_name(unsigned int id, const Name &name)
{
#ifndef NDEBUG
	std::cout << "Registering name " << id << std::endl;
#endif
	// this is an internal method which is called to insert a NEW name
	unsigned int not_used;
	assert(this->resolve(name,not_used) == -1);
	this->id_to_name.insert(std::make_pair(id, name));
	this->name_to_id.insert(std::make_pair(name,id));
	// add a new log entry
	std::stringstream buf;
	push_u32(buf, id);
	push_u32(buf, name.ip);
	push_u32(buf, name.port);
	LogEntry log = { NEW_NODE, buf.str() };
	this->logs.push_back(log);
	return id;
}

void NameService::remove_name(unsigned int id)
{
	RightMap::iterator it = this->id_to_name.find(id);
	// this is an internal method which is called to remove an EXISTING name
	assert(it != this->id_to_name.end());
	this->name_to_id.erase(it->second);
	this->id_to_name.erase(id);
}

void NameService::kill(unsigned int id)
{
	this->remove_name(id);
	// add a log entry
	std::stringstream buf;
	buf << htonl(id);
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
	return lhs.name < rhs.name && lhs.to_signiture() < rhs.to_signiture();
}

// needed for std::map
bool operator< (const Name& lhs, const Name& rhs)
{
	return lhs.ip < rhs.ip && lhs.port < rhs.port;
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

unsigned int NameService::get_version() const
{
	return this->logs.size();
}

std::string format_arg(int arg_type)
{
	std::stringstream ss;
	int data_type = get_arg_data_type(arg_type);

	switch(data_type)
	{
		case ARG_CHAR:
			ss << "CHAR ";
			break;

		case ARG_SHORT:
			ss << "SHORT ";
			break;

		case ARG_INT:
			ss << "INT ";
			break;

		case ARG_LONG:
			ss << "LONG ";
			break;

		case ARG_DOUBLE:
			ss << "DOUBLE ";
			break;

		case ARG_FLOAT:
			ss << "FLOAT ";
			break;

		default:
			// unreachable
			assert(false);
			break;
	}

	if(is_arg_input(arg_type))
	{
		ss << "input ";
	}

	if(is_arg_output(arg_type))
	{
		ss << "output ";
	}

	size_t size;

	if(is_arg_array(arg_type, size))
	{
		ss << "array(" << size << ") ";
	}
	else
	{
		assert(size == 0);
		ss << "scalar ";
	}

	return ss.str();
}

void print_function(Function &func)
{
	std::cout << func.name << std::endl;

	for(size_t i = 0; i < func.types.size(); i++)
	{
		std::cout << '\t' << format_arg(func.types[i]) << std::endl;
	}

	std::cout << std::endl;
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
