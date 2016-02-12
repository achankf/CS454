#include "common.hpp"
#include <netdb.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <algorithm>
#include <utility>
#include <cassert>

int TCP::Socket::create_socket()
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

TCP::Socket::Socket() : local_fd(-1)
{
}

TCP::Socket::~Socket()
{
	for(std::set<int>::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		// close connections
		close(*it);
	}
}

int TCP::Socket::bind_and_listen(int port, int num_listen)
{
	int temp_fd;
	temp_fd = this->create_socket();

	if(temp_fd < 0)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	struct sockaddr_in socket_info = {0};

	socket_info.sin_family = AF_INET;

	socket_info.sin_addr.s_addr = htonl(INADDR_ANY);

	socket_info.sin_port = htons(port);

	if(bind(temp_fd, (struct sockaddr*) &socket_info, sizeof(socket_info)) < 0)
	{
		// this should not happen in the student environment
		assert(false);
		close(temp_fd);
		return -1;
	}

	if(listen(temp_fd, num_listen) < 0)
	{
		// this should not happen in the student environment
		assert(false);
		close(temp_fd);
		return -1;
	}

	this->local_fd = temp_fd;
	this->connected_fds.insert(this->local_fd);
	return 0;
}

void TCP::Socket::get_name(std::string *ipv4, int *port) const
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	int retval = getsockname(this->local_fd, (struct sockaddr *)&sin, &len);
	// not going to deal with ERRNO
	assert(retval >= 0);
	char buf[1024];
	retval = gethostname(buf, sizeof(buf));
	// not going to deal with ERRNO
	assert(retval >= 0);
	*ipv4 = std::string(buf);
	*port = ntohs(sin.sin_port);
}

int TCP::Socket::get_message(int dst_fd, int &ret)
{
#if 0
	char buf[1024];
	ssize_t count = read(this->socket_fd, buf, sizeof(buf));
	Requests::iterator it;
	it = this->read_buf.find(dst_fd);

	if(it == this->read_buf.end())
	{
		//TODO
		return -1;
	}

	Message &msg = it->second;

	for(ssize_t i = 0; i < count; i++)
	{
		msg.push_back(buf[i]);
	}

	if(msg.empty())
	{
		return -1;
	}

	ret = msg.front();
	msg.pop_front();
#endif
	return 0;
}

#define BUFFER_THRESHOLD 1024

int TCP::Socket::push_message(int dst_fd, int c)
{
	Requests::iterator it;
	it = this->write_buf.find(dst_fd);

	if(it == this->write_buf.end())
	{
		std::pair<Requests::iterator, bool> ret = this->write_buf.insert(std::make_pair(dst_fd, Message()));
		// we just checked that dst_fd is not in the buffer...
		assert(ret.second == false);
		it = ret.first;
	}

	Message &msg = it->second;
	msg.push_back(c);

	if(msg.size() >= BUFFER_THRESHOLD)
	{
		return this->flush_write(dst_fd);
	}

	return 0;
}

int TCP::Socket::flush_write(int dst_fd)
{
#if 0
	Requests::iterator it;
	it = this->write_buf.find(dst_fd);

	if(it == this->write_buf.end())
	{
		//TODO dst_fd should be removed; maybe there's something wrong
		assert(false);
		return 0;
	}

	Message &msg = it->second;
	size_t size = msg.size();
	char *data = new char[size];
	std::copy(msg.begin(), msg.end(), data);
	int count = write(this->socket_fd, data, size);
	delete [] data;
	return count;
#endif
	return 0;
}

void TCP::Socket::setup_read_fds(fd_set &fds)
{
	// similar to write fds, except read fds also need socket_fd (to check for incoming connections)
	this->setup_write_fds(fds);
	FD_SET(this->local_fd, &fds);
}

void TCP::Socket::setup_write_fds(fd_set &fds)
{
	FD_ZERO(&fds);

	for(std::set<int>::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		if(*it != this->local_fd)
		{
			FD_SET(*it, &fds);
		}
	}
}

int TCP::Socket::sync()
{
	setup_read_fds(this->readfds);
	setup_write_fds(this->writefds);
	int max_fd = this->get_max_fd();
	int retval = select(max_fd + 1, &this->readfds, &this->writefds, NULL, NULL);

	if(retval < 0 && errno != EINTR)
	{
		// some error occurred
		return retval;
	}

	for(std::set<int>::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		int fd = *it;

		if(FD_ISSET(fd, &this->readfds))
		{
			if(fd == this->local_fd)
			{
				// in other words, this is the server and is listening for new client connections
				// -1 means the server hasn't called bind_and_listen
				assert(this->local_fd != -1);
				int clientfd = accept(this->local_fd, NULL, NULL);

				if(clientfd < 0)
				{
					// this should not happen in the student environment
					assert(false);
					return -1;
				}

				this->connected_fds.insert(clientfd);
			}
			else
			{
				// sockets for remote connections
			}
		}

		if(FD_ISSET(fd, &this->writefds))
		{
		}
	}

	return 0;
}

int TCP::Socket::get_max_fd() const
{
	if(!this->connected_fds.empty())
	{
		// since set is sorted, the rightmost item is the largest item
		int max_fd = *this->connected_fds.rbegin();
		// double-check just in case
		assert(max_fd == *std::max_element(this->connected_fds.begin(), this->connected_fds.end()));
		return max_fd;
	}

	// this should not happen
	assert(false);
	return -1;
}

/** Turn hostname into its IP address */
static int map_hostname(char *hostname, int *ip)
{
	struct hostent* server_entity;
	server_entity = gethostbyname(hostname);

	if(server_entity == NULL)
	{
		return -1;
	}

	memcpy(ip, server_entity->h_addr, server_entity->h_length);
	return 0;
}

int TCP::Socket::connect_remote(char *hostname, int port)
{
	int temp_fd = this->create_socket();
	// resolve IP address
	int ip;
	{
		struct hostent* server_entity;
		server_entity = gethostbyname(hostname);

		if(server_entity == NULL)
		{
			close(temp_fd);
			return -1;
		}

		memcpy(&ip, server_entity->h_addr, server_entity->h_length);
	}
	struct sockaddr_in remote_info = {0};
	remote_info.sin_family = AF_INET;
	remote_info.sin_port = htons(port);
	memcpy(&remote_info.sin_addr, &ip, sizeof(int));

	if(connect(temp_fd, (struct sockaddr *)&remote_info, sizeof(remote_info)) < 0)
	{
		// should not happen in the student environment
		assert(false);
		return -1;
	}

	// connect to remote machine (server) successfully)
	this->connected_fds.insert(temp_fd);
	return 0;
}
