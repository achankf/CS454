#include "common.hpp"
#include "name_service.hpp" // struct Name
#include "debug.hpp"
#include "sockets.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <unistd.h>

void print_host_info(int fd, const char *prefix)
{
	std::string hostname;
	Name name;
	get_hostname(fd, hostname, name);
	std::cout << prefix << "_ADDRESS " << hostname << std::endl
	          << prefix << "_PORT " << name.port << std::endl;
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

static Name sockaddr_in_to_name(struct sockaddr_in &sin)
{
	Name ret;
	// convert the ip address into host byte order
	ret.ip = ntohl(sin.sin_addr.s_addr);
	ret.port = ntohs(sin.sin_port);
	return ret;
}

void get_hostname(int fd, std::string &hostname, Name &name)
{
	// get ip address and port number
	{
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		int retval = getsockname(fd, (struct sockaddr *)&sin, &len);
		// supress warning when compiling with NDEBUG
		(void) retval;
		// not going to deal with ERRNO
		assert(retval >= 0);
		name = sockaddr_in_to_name(sin);
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
	ret = sockaddr_in_to_name(sin);
	return 0;
}

void push_u32(std::stringstream &ss, unsigned val)
{
	val = htonl(val);
	ss.write((char*)&val, 4);
}

unsigned pop_u32(std::stringstream &ss)
{
	// assumes ss contains the valid bytes
	unsigned ret;
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

void push_string(std::stringstream &ss, const std::string &str)
{
	push_u32(ss, str.size());
	ss << str;
}

std::string pop_string(std::stringstream &ss)
{
	unsigned size = pop_u32(ss);
	return raw_read(ss, size);
}
