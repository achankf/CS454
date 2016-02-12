#include <string>
#include <cassert>
#include "common.hpp"
#include <iostream>
#include <sys/select.h>

void cap_first_letter(std::string &str)
{
	bool is_prev_space = true;

	for(size_t i = 0; i < str.size(); i++)
	{
		if(isspace(str[i]))
		{
			// no need to mutate any space characters
			is_prev_space = true;
			continue;
		}

		if(is_prev_space)
		{
			// previous character was a space but the current one isn't, so turn the current character to upper case
			str[i] = toupper(str[i]);
		}
		else
		{
			str[i] = tolower(str[i]);
		}

		is_prev_space = false;
	}
}

void test_cap()
{
#ifndef NDEBUG
	// empty string
	std::string str = "";
	cap_first_letter(str);
	assert(str == "");
	// one word
	str = "helLo";
	cap_first_letter(str);
	assert(str == "Hello");
	// two word
	str = "hello wORld";
	cap_first_letter(str);
	assert(str == "Hello World");
	// spaces
	str = "    ";
	cap_first_letter(str);
	assert(str == "    ");
	// sentences
	str = " How are you?";
	cap_first_letter(str);
	assert(str == " How Are You?");
	// example
	str = "document specification FOR CS454 a2 milestone";
	cap_first_letter(str);
	assert(str == "Document Specification For Cs454 A2 Milestone");
#endif
}

int main()
{
	test_cap();
	TCP::Socket socket;

	if(socket.bind_and_listen() < 0)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}

	int port;
	std::string address;
	socket.get_name(&address, &port);
	std::cout << "SERVER_ADDRESS " << address << std::endl;
	std::cout << "SERVER_PORT " << port << std::endl;
	int i =0;

	while(true)
	{
		socket.sync();
	}

	// unreachable
	assert(false);
}
