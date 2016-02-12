#include <string>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include "common.hpp"

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
	TCP::Socket socket;

	if(socket.connect_remote(hostname, port) < -1)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}
}
