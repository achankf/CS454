#ifndef _channel_hpp_
#define _channel_hpp_

#include <map>
#include <pthread.h>
#include <string>

namespace TCP
{
class Sockets;

// all public methods are synchronized by a mutex
class StringChannel
{
private: // typedefs
	struct StringRequest
	{
		unsigned int size;
		std::string data;
	};
	typedef std::map<int, StringRequest> Requests;

private: // data
	Sockets &socket_ref;
	Requests requests;
	pthread_mutex_t lock;
	bool closing;
	int num_pending;

private: // helpers

	// unsynchronized helper
	int receive_any_helper(std::pair<int,std::string> &ret);

public:
	StringChannel(Sockets &socket_ref);
	~StringChannel();

	// synchronous send/receive
	int receive_any(std::pair<int,std::string> &ret);
	void send(int dst_fd, std::string &str);

	// synchronous version of Sockets::sync()
	int sync();

	// effectively needs an infinite loop; this method does not block
	bool is_closing();
	void close();
};

}

#endif
