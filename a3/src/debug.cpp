#include "debug.hpp"
#include "name_service.hpp"
#include "rpc.h"
#include <cassert>
#include <sstream>

#ifndef NDEBUG
#include <iostream>
#endif

void debug_print_type(const Postman::Request &req)
{
#ifndef NDEBUG
	std::string temp;

	switch(req.message.msg_type)
	{
		case Postman::HELLO:
			temp = "Request: HELLO";
			break;

		case Postman::GOOD_TO_SEE_YOU:
			temp = "Reply: GOOD_TO_SEE_YOU";
			break;

		case Postman::REGISTER:
			temp = "Request: REGISTER";
			break;

		case Postman::LOC_REQUEST:
			temp = "Request: LOC_REQUEST";
			break;

		case Postman::LOC_SUCCESS:
			temp="Request: LOC_SUCCESS";
			break;

		case Postman::LOC_FAILURE:
			temp = "Request: LOC_FAILURE";
			break;

		case Postman::EXECUTE:
			temp = "Request: EXECUTE";
			break;

		case Postman::EXECUTE_SUCCESS:
			temp = "Request: EXECUTE_SUCCESS";
			break;

		case Postman::EXECUTE_FAILURE:
			temp = "Request: EXECUTE_FAILURE";
			break;

		case Postman::TERMINATE:
			temp = "Request: TERMINATE";
			break;

		default:
			// unreachable
			assert(false);
			break;
	}

	std::cout << temp << std::endl;
#endif
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

std::string to_ipv4_string(int bin)
{
	std::stringstream ss;
	ss << (unsigned)((bin >> 24) & 0xff) << '.'
	   << (unsigned)((bin >> 16) & 0xff) << '.'
	   << (unsigned)((bin >> 8) & 0xff) << '.'
	   << (unsigned)(bin & 0xff);
	return ss.str();
}
