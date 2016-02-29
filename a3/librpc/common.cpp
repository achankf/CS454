#include "common.hpp"
#include "name_service.hpp" // struct Name
#include "sockets.hpp"
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
	// copy the ip address (in network byte order) into an int
	memcpy(&sin.sin_addr, &ret.ip, sizeof(ret.ip));
	// convert the ip address into host byte order
	ret.ip = ntohl(ret.ip);
	ret.port = ntohs(sin.sin_port);
	return ret;
}

void get_hostname(int fd, std::string &hostname, Name &name)
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	int retval = getsockname(fd, (struct sockaddr *)&sin, &len);
	// supress warning when compiling with NDEBUG
	(void) retval;
	// not going to deal with ERRNO
	assert(retval >= 0);
	name = sockaddr_in_to_name(sin);
	char buf[1024];
	retval = gethostname(buf, sizeof(buf));
	// not going to deal with ERRNO
	assert(retval >= 0);
	hostname = std::string(buf);
}

int get_peer_info(int fd, Name &ret)
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);

	if(getsockname(fd, (struct sockaddr *)&sin, &len) < 0)
	{
		//TODO
		assert(false);
		return -1;
	}

	ret = sockaddr_in_to_name(sin);
	return 0;
}

std::stringstream &push_u32(std::stringstream &ss, unsigned int val)
{
	val = htonl(val);
	ss << (unsigned char)(val >> 24);
	ss << (unsigned char)(val >> 16);
	ss << (unsigned char)(val >> 8);
	ss << (unsigned char)val;
	return ss;
}

unsigned int pop_uint32(std::stringstream &ss)
{
	// assumes ss contains the valid bytes
	unsigned char b1, b2, b3, b4;
	ss >> b1 >> b2 >> b3 >> b4;
	unsigned int ret = 0;
	ret += b1 << 24;
	ret += b1 << 16;
	ret += b1 << 8;
	ret += b1;
	return ntohl(ret);
}

std::string get_unformatted(std::stringstream &ss, size_t size)
{
	char buf[size+1];
	ss.get(buf, sizeof buf);
	return buf;
}
