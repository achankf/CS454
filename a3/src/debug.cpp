#include "debug.hpp"
#include <cassert>

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
