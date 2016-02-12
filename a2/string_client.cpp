#include <cassert>
#include <cstdlib>
#include <iostream>
#include "sockets.hpp"

int main()
{
	char *hostname = getenv("SERVER_ADDRESS");

	if(hostname == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}

	char *port_chars = getenv("SERVER_PORT");

	if(port_chars == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}

	int port = atoi(port_chars);
	std::cout << hostname << " " << port << std::endl;
	TCP::Sockets socket;
	int server_fd = socket.connect_remote(hostname, port);

	if(server_fd < 0)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}

	std::cout << "server_fd: " << server_fd << std::endl;
	TCP::Sockets::Message &msg = socket.get_write_buf(server_fd);
	msg.push_back('a');
	msg.push_back('l');
	msg.push_back('f');
	msg.push_back('r');
	msg.push_back('e');
	msg.push_back('d');
	msg.push_back(' ');
	msg.push_back('c');
	msg.push_back('h');
	msg.push_back('a');
	msg.push_back('n');

	while(true)
	{
		socket.sync();
		std::pair<int,TCP::Sockets::Message*> request;

		if(socket.get_available_msg(request) >= 0)
		{
			std::string str;
			TCP::Sockets::Message &msg = *request.second;
			std::copy(msg.begin(), msg.end(), std::inserter(str, str.end()));
			std::cout << "Server: " << str << std::endl;
			// clear the buffer
			msg = TCP::Sockets::Message();
		}
	}
}
