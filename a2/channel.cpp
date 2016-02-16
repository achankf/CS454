#include "channel.hpp"
#include "sockets.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <exception>
#include <limits>
#include <unistd.h>

namespace TCP
{

StringChannel::StringChannel(Sockets &socket_ref)
	: socket_ref(socket_ref), closing(false), num_pending(0)
{
	if(pthread_mutex_init(&lock, NULL) != 0)
	{
		// should not happen in the student environment
		assert(false);
		throw std::exception();
	}
}

StringChannel::~StringChannel()
{
	pthread_mutex_destroy(&this->lock);
}

int StringChannel::receive_any(std::pair<int,std::string> &ret)
{
	int retval;
	pthread_mutex_lock(&this->lock);
	retval = receive_any_helper(ret);
	pthread_mutex_unlock(&this->lock);

	if(retval >= 0)
	{
		// reduce the number of pending request count by 1
		this->num_pending--;
		assert(retval >= 0);
	}

	return retval;
}

int StringChannel::receive_any_helper(std::pair<int,std::string> &ret)
{
	std::pair<int,Sockets::Message*> raw_request;

	if(this->socket_ref.get_available_msg(raw_request) < 0)
	{
		// no request is ready yet (still buffering)
		return -1;
	}

	StringRequest &request = this->requests[raw_request.first];
	Sockets::Message &msg = *raw_request.second;

	if(request.data.empty())
	{
		// this is a new request
		// nsize in network order
		unsigned int nsize = msg.front() << 24;
		msg.pop_front();
		nsize += msg.front() << 16;
		msg.pop_front();
		nsize += msg.front() << 8;
		msg.pop_front();
		nsize += msg.front();
		msg.pop_front();
		// request.size in host order
		request.size = ntohl(nsize);
	}

	// msg must have at least 1 byte to read (i.e. '\0')
	assert(!msg.empty());
	// the request isn't new -- fill up data up to request.size - 1
	// notice we're working on std::string, not a c-string
	size_t size = request.size - 1;

	while(!msg.empty())
	{
		if(request.data.size() == size)
		{
#ifndef NDEBUG
			// the next character must be the null terminator
			assert(!msg.empty());
			unsigned char null_char = msg.front();
			assert(null_char == '\0');
#endif
			// get rid of the null-terminator
			msg.pop_front();
			// assign return value (by copy)
			ret.first = raw_request.first;
			ret.second = request.data;
			// clear the current buffer for new requests
			int num_erased = requests.erase(raw_request.first);
			// suppress warning
			(void) num_erased;
			assert(num_erased == 1);

			if(msg.empty())
			{
				// calling clear() seems to deallocate memory (tested on gcc 4.6.4)
				// note: msg allocates memory dynamically, so need to shrink memory
				// when available to prevent internal memory fragmentation
				msg.clear();
			}

			return 0;
		}

		request.data.push_back(msg.front());
		msg.pop_front();
	}

	return -1;
}

void StringChannel::send(int dst_fd, std::string &str)
{
	if(str.size() > std::numeric_limits<unsigned int>::max() - 1)
	{
		// the request is too large, don't process it since the header only has 4 bytes
		return;
	}

	// do the copying here since it doesn't require mutual exclusion
	const char *c_str = str.c_str();
	assert(c_str[str.size()] == '\0');
	pthread_mutex_lock(&this->lock);
	{
		Sockets::Message &send = this->socket_ref.get_write_buf(dst_fd);
		// format: (4-byte size of message, message)
		unsigned int nsize = htonl(static_cast<unsigned int>(str.size() + 1));
		send.push_back(nsize >> 24);
		send.push_back(nsize >> 16);
		send.push_back(nsize >> 8);
		send.push_back(nsize);
		std::copy(c_str, c_str + (str.size()+1), std::inserter(send, send.end()));
	}
	pthread_mutex_unlock(&this->lock);
	// increase the number of pending requests by 1
	this->num_pending++;
}

int StringChannel::sync()
{
	pthread_mutex_lock(&this->lock);
	int retval = this->socket_ref.sync();
	pthread_mutex_unlock(&this->lock);
	return retval;
}

bool StringChannel::is_closing()
{
	bool retval;
	int num_pending_temp;
	pthread_mutex_lock(&this->lock);
	retval = this->closing;
	num_pending_temp = this->num_pending;
	pthread_mutex_unlock(&this->lock);
	return retval && num_pending_temp == 0;
}

void StringChannel::close()
{
	pthread_mutex_lock(&this->lock);
	this->closing = true;
	pthread_mutex_unlock(&this->lock);
}

}
