#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp" // struct Name
#include "sockets.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <netdb.h>
#include <unistd.h>

void print_host_info(int fd, const char *prefix)
{
	std::string hostname;
	int port;
	get_hostname(fd, hostname, port);
	std::cout << prefix << "_ADDRESS " << hostname << std::endl
	          << prefix << "_PORT " << port << std::endl;
}

int connect_to_binder(TCP::Sockets &sockets)
{
	char *hostname = getenv("BINDER_ADDRESS");

	if(hostname == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	char *port_chars = getenv("BINDER_PORT");

	if(hostname == NULL)
	{
		// this should not happen in the student environment
		assert(false);
		return -1;
	}

	int port = atoi(port_chars);
	return sockets.connect_remote(hostname, port);
}

void get_hostname(int fd, std::string &hostname, int &port)
{
	// get port number
	{
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		getsockname(fd, (struct sockaddr *)&sin, &len);
		port = ntohs(sin.sin_port);
	}
	// get hostname
	{
		char buf[1024];
		int retval = gethostname(buf, sizeof(buf));
		(void) retval;
		// not going to deal with ERRNO
		assert(retval >= 0);
		hostname = std::string(buf);
	}
}

int get_peer_info(int fd, Name &ret)
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	if(getpeername(fd, (struct sockaddr *)&sin, &len) < 0)
	{
		// should not happen in the student environment?
		assert(false);
		return -1;
	}

	// assign name
	ret.ip = sin.sin_addr.s_addr; // DON'T CONVERT THIS INTO HOST ORDER
	ret.port = ntohs(sin.sin_port);
	return 0;
}

void push_i32(std::stringstream &ss, int val)
{
	val = htonl(val);
	ss.write((char*)&val, 4);
}

int pop_i32(std::stringstream &ss)
{
	// assumes ss contains the valid bytes
	int ret;
	ss.read((char*)&ret, 4);
	return ntohl(ret);
}

std::string raw_read(std::stringstream &ss, size_t size)
{
	char *buf = new char[size+1];

	if(!ss.read(buf, size))
	{
		assert(false);
		// partially read... bug...
	}

	std::string ret;
	// copy character-by-character
	std::copy(buf, (buf + size), std::inserter(ret, ret.end()));
	delete []buf;
	return ret;
}

void push(std::stringstream &ss, const std::string &str)
{
	push_i32(ss, str.size());
	ss << str;
}

std::string pop_string(std::stringstream &ss)
{
	unsigned size = pop_i32(ss);
	return raw_read(ss, size);
}

Timer::Timer(int timeout_in_seconds)
	: start(std::clock()),
	  timeout_in_seconds(timeout_in_seconds) {}

bool Timer::is_timeout() const
{
	float duration = clock() - this->start;
	return (duration / CLOCKS_PER_SEC) >= this->timeout_in_seconds;
}

void push_i8(std::stringstream &ss, char val)
{
	ss.write(&val, 1);
}

void push_i16(std::stringstream &ss, short val)
{
	val = htons(val);
	ss.write((char*)&val, 2);
}

void push_i64(std::stringstream &ss, long val)
{
	int num = 0x01;
	char *data = (char*)&num;
	char buf[8];

	if(*data == 0x01) // test the first byte
	{
		// little endian -- convert to big endian
		buf[0] = data[7];
		buf[1] = data[6];
		buf[2] = data[5];
		buf[3] = data[4];
		buf[4] = data[3];
		buf[5] = data[2];
		buf[6] = data[1];
		buf[7] = data[0];
		ss.write((char*)&buf, 8);
	}
	else
	{
		// big endian -- just pass it
		ss.write((char*)&val, 8);
	}
}

void push_f32(std::stringstream &ss, float val)
{
	//TODO hope that it works...
	ss.write((char*)&val, 4);
}

void push_f64(std::stringstream &ss, double val)
{
	//TODO hope that it works...
	ss.write((char*)&val, 8);
}
