#include "channel.hpp"
#include "sockets.hpp"
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iostream>
#include <pthread.h>

// packed as arguments for handle_requests
struct SocketReference
{
	int server_fd;
	TCP::Sockets &socket;
	TCP::StringChannel &channel;
};

static void *handle_requests(void *data)
{
	SocketReference &ref = *static_cast<SocketReference *>(data);
	TCP::StringChannel &channel = ref.channel;

	while(!channel.is_closing())
	{
		std::pair<int,std::string> request;
		channel.sync();

		if(channel.receive_any(request) < 0)
		{
			continue;
		}

		assert(request.first == ref.server_fd);
		std::cout << "Server: " << request.second << std::endl;
	}

	return NULL;
}

static int setup_client(TCP::Sockets &socket)
{
	char *hostname = getenv("SERVER_ADDRESS");

	if(hostname == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	char *port_chars = getenv("SERVER_PORT");

	if(port_chars == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	int port = atoi(port_chars);
	return socket.connect_remote(hostname, port);
}

int main()
{
	TCP::Sockets socket;
	int server_fd = setup_client(socket);

	if(server_fd < 0)
	{
		// this should not happen in the student environment
		assert(false);
		return 1;
	}

	TCP::StringChannel channel(socket);
	SocketReference ref = {server_fd, socket, channel};
	pthread_t net_sync_thread;

	if(pthread_create(&net_sync_thread, NULL, &handle_requests, static_cast<void*>(&ref)) != 0)
	{
		// should not happen in the student environment
		assert(false);
		return 1;
	}

	// get lines and send them through the channel as requests
	for(std::string line; std::getline(std::cin, line);)
	{
		channel.send(server_fd, line);
		// wait for 2 seconds per request, as required by the assignment specs
		sleep(2);
	}

	channel.close();

	if(pthread_join(net_sync_thread, NULL) != 0)
	{
		// should not happen in the student environment
		assert(false);
		return 1;
	}
}
