#include "sockets.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <unistd.h>

int TCP::Sockets::create_socket()
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

TCP::Sockets::Sockets() : local_fd(-1)
{
}

TCP::Sockets::~Sockets()
{
	for(Fds::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		int fd = *it;

		if(fd != this->local_fd)
		{
			// close connections
			close(fd);
		}
	}

	close(this->local_fd);
}

int TCP::Sockets::bind_and_listen(int port, int num_listen)
{
	int temp_fd;
	temp_fd = this->create_socket();

	if(temp_fd < 0)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	struct sockaddr_in socket_info;

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
	this->connected_fds.insert(temp_fd);
	return temp_fd;
}

void TCP::Sockets::get_name(std::string *hostname, int *port) const
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
	*hostname = std::string(buf);
	*port = ntohs(sin.sin_port);
}

void TCP::Sockets::setup_read_fds(fd_set &fds) const
{
	// similar to write fds, except read fds also need socket_fd (to check for incoming connections)
	this->setup_write_fds(fds);
	FD_SET(this->local_fd, &fds);
}

void TCP::Sockets::setup_write_fds(fd_set &fds) const
{
	FD_ZERO(&fds);

	for(Fds::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		if(*it != this->local_fd)
		{
			FD_SET(*it, &fds);
		}
	}
}

int TCP::Sockets::sync()
{
	fd_set readfds, writefds;
	setup_read_fds(readfds);
	setup_write_fds(writefds);
	int max_fd = this->get_max_fd();
	char buf[2048];
	int retval = select(max_fd + 1, &readfds, &writefds, NULL, NULL);

	if(retval < 0 && errno != EINTR)
	{
		// some error occurred
		return retval;
	}

	for(Fds::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		int fd = *it;

		if(FD_ISSET(fd, &readfds))
		{
			// in other words, this is the server and is listening for new client connections
			// -1 means the server hasn't called bind_and_listen
			assert(this->local_fd != -1 || this->local_fd != fd);

			if(fd == this->local_fd)
			{
				int clientfd = accept(this->local_fd, NULL, NULL);

				if(clientfd < 0)
				{
					// this should not happen in the student environment
					assert(false);
					return -1;
				}

				bool inserted = this->connected_fds.insert(clientfd).second;
				// fds must be unique; something is wrong here
				assert(inserted);
			}
			else
			{
				// sockets for remote connections
				size_t count = read(fd, buf, sizeof(buf));

				if(count == 0)
				{
					// remote sent EOF -- disconnect remote
					this->disconnect(fd);
				}
				else
				{
					// copy message to a buffer
					Message &msg = get_read_buf(fd);
					std::copy(buf, buf+count, std::inserter(msg, msg.end()));
					// msg could have stored requests that haven't been sent
					assert(msg.size() >= count);
				}
			}
		}

		if(FD_ISSET(fd, &writefds))
		{
			// should only write to a REMOTE connection
			// if local_fd isn't set (i.e. client doesn't bind and listen), then local_fd should be -1 and the assertion should always hold
			assert(fd != this->local_fd);
			Message &msg = this->get_write_buf(fd);
			assert(fd != this->local_fd);

			if(!msg.empty())
			{
				size_t i;

				for(i = 0; i < sizeof buf && !msg.empty(); i++)
				{
					buf[i] = msg.front();
					msg.pop_front();
				}

				if(write(fd, buf, i) < 0)
				{
					// this can happen when the remote shutdown before the reply was sent
					// for example, running valgrind can slow the server significantly and this can happen
					return -1;
				}
			}
		}
	}

	return 0;
}

int TCP::Sockets::get_max_fd() const
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

int TCP::Sockets::connect_remote(char *hostname, int port)
{
	int temp_fd = this->create_socket();
	// resolve IP address
	int ip;
	{
		struct hostent* server_entity;
		server_entity = gethostbyname(hostname);

		if(server_entity == NULL)
		{
			// should not happen in the student environment
			assert(false);
			close(temp_fd);
			return -1;
		}

		memcpy(&ip, server_entity->h_addr, server_entity->h_length);
	}
	struct sockaddr_in remote_info;
	remote_info.sin_family = AF_INET;
	remote_info.sin_port = htons(port);
	memcpy(&remote_info.sin_addr, &ip, sizeof(int));

	if(connect(temp_fd, (struct sockaddr *)&remote_info, sizeof(remote_info)) < 0)
	{
		// should not happen in the student environment
		assert(false);
		close(temp_fd);
		return -1;
	}

	// connect to remote machine (server) successfully)
	this->connected_fds.insert(temp_fd);
	return temp_fd;
}

void TCP::Sockets::disconnect(int fd)
{
	Fds::iterator it = this->connected_fds.find(fd);
	// this method should only be called once per fd
	assert(it != this->connected_fds.end());
	close(fd);
	this->connected_fds.erase(it);
}

TCP::Sockets::Message &TCP::Sockets::get_read_buf(int dst_fd)
{
	return this->read_buf[dst_fd];
}

TCP::Sockets::Message &TCP::Sockets::get_write_buf(int dst_fd)
{
	return this->write_buf[dst_fd];
}

int TCP::Sockets::get_available_msg(std::pair<int,Message*> &ret)
{
	Requests::iterator it = this->read_buf.begin();

	for(; it != this->read_buf.end(); it++)
	{
		if(!it->second.empty())
		{
			ret.first = it->first;
			ret.second = &it->second;
			return 0;
		}
	}

	return -1;
}
