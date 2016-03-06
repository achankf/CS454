#include "common.hpp"
#include "sockets.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>

int TCP::Sockets::create_socket()
{
	return socket(AF_INET, SOCK_STREAM, 0);
}

TCP::Sockets::Sockets()
	: local_fd(-1),
	  buffer(NULL)
{
}

TCP::Sockets::~Sockets()
{
	for(Fds::iterator it = this->connected_fds.begin(); it != this->connected_fds.end(); it++)
	{
		int fd = *it;
		assert(fd != -1);

		if(fd != this->local_fd)
		{
			// close connections
			close(fd);
		}
	}

	if(this->local_fd != -1)
	{
		close(this->local_fd);
	}
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
	if(num_connected() == 0)
	{
		return 0;
	}

	fd_set readfds, writefds;
	setup_read_fds(readfds);
	setup_write_fds(writefds);
	int max_fd = this->get_max_fd();
	char buf[SOCKET_BUF_SIZE];
	struct timeval time;
	time.tv_sec = 1;
	time.tv_usec = 0;
	int retval = select(max_fd + 1, &readfds, &writefds, NULL, &time);

	if(retval < 0 && errno != EINTR)
	{
		// some error occurred
		return retval;
	}

	Fds local_copy;
	local_copy = this->connected_fds;

	for(Fds::iterator it = local_copy.begin(); it != local_copy.end(); it++)
	{
		int fd = *it;

		if(FD_ISSET(fd, &readfds))
		{
			// in other words, this is the server and is listening for new client connections
			// -1 means the server hasn't called bind_and_listen
			assert(this->local_fd != -1 || this->local_fd != fd);

			if(fd == this->local_fd)
			{
				int remote_fd = accept(this->local_fd, NULL, NULL);

				if(remote_fd < 0)
				{
					// this should not happen in the student environment
					assert(false);
					return -1;
				}

				bool inserted = this->connected_fds.insert(remote_fd).second;
				// supress warning when compiling with NDEBUG
				(void) inserted;
				// fds must be unique; something is wrong here
				assert(inserted);
#ifndef NDEBUG
				std::cout << "connected " << remote_fd << std::endl;
#endif
			}
			else
			{
				int count = read(fd, buf, sizeof(buf));

				if(count == 0)
				{
					// remote sent EOF -- disconnect remote
					this->disconnect(fd);
					break; // iterators invalidated
				}
				else if(count > 0)
				{
					std::string buf_str(buf, count);

					if(this->buffer != NULL)
					{
						// notify the buffer
						this->buffer->read_avail(fd, buf_str);
					}
				}
			}
		}

		if(FD_ISSET(fd, &writefds))
		{
			if(this->connected_fds.find(fd) != this->connected_fds.end())
			{
				// double-check in case the fd is closed
				// ignore return value
				this->flush(fd);
			}
		}
	}

	return 0;
}

int TCP::Sockets::flush(int dst_fd)
{
	// should only write to a REMOTE connection
	// if local_fd isn't set (i.e. client doesn't bind and listen), then local_fd should be -1 and the assertion should always hold
	assert(dst_fd != this->local_fd);

	if(this->buffer == NULL)
	{
		// no buffer set -- coding error
		assert(false);
		return 0;
	}

	const std::string msg = this->buffer->write_avail(dst_fd);

	if(msg.empty())
	{
		// have nothing to send
		return -1;
	}

	char *buf = new char[msg.size()];
	memcpy(buf, msg.c_str(), msg.size());
	size_t num_written = write(dst_fd, buf, msg.size());
	delete [] buf;

	if(num_written == msg.size())
	{
		return 0;
	}

	// for example, running valgrind can slow the server significantly and this can happen
	// though, this probably won't happen for the assignment, so I'll ignore error handling
	return -1;
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

int TCP::Sockets::connect_remote(const char *hostname, int port)
{
	// resolve IP address
	int ip;
	struct hostent* server_entity;
	server_entity = gethostbyname(hostname);

	if(server_entity == NULL)
	{
		// should not happen in the student environment
		assert(false);
		return -1;
	}

	memcpy(&ip, server_entity->h_addr, server_entity->h_length);
	return this->connect_remote(ip, port);
}

int TCP::Sockets::connect_remote(int ip, int port)
{
	struct sockaddr_in remote_info;
	remote_info.sin_family = AF_INET;
	remote_info.sin_port = htons(port);
	memcpy(&remote_info.sin_addr, &ip, sizeof(int));
	int temp_fd = this->create_socket();

	if(temp_fd < 0)
	{
		assert(false);
		return -1;
	}

	if(connect(temp_fd, (struct sockaddr *)&remote_info, sizeof(remote_info)) < 0)
	{
		close(temp_fd);
		return -1;
	}

	// connect to remote machine (server) successfully)
	this->connected_fds.insert(temp_fd);
	return temp_fd;
}

void TCP::Sockets::disconnect(int fd)
{
#ifndef NDEBUG
	std::cout << "disconnecting " << fd << std::endl;
#endif
	Fds::iterator it = this->connected_fds.find(fd);

	if(it == this->connected_fds.end())
	{
		// do nothing since the fd doesn't represent a connected socket
		return;
	}

	this->connected_fds.erase(it);
	close(fd);
}

void TCP::Sockets::set_buffer(DataBuffer *buffer)
{
	this->buffer = buffer;
}

bool TCP::Sockets::is_alive(int fd) const
{
	return this->connected_fds.find(fd) != this->connected_fds.end();
}

size_t TCP::Sockets::num_connected(int *exclude_fd) const
{
	if(exclude_fd != NULL && this->is_alive(*exclude_fd))
	{
		// target found, so exclude it in the count
		return this->connected_fds.size() - 1;
	}

	return this->connected_fds.size();
}
