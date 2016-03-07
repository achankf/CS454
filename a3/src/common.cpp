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
		return CANNOT_RESOLVE_HOSTNAME;
	}

	// assign name
	ret.ip = sin.sin_addr.s_addr; // DON'T CONVERT THIS INTO HOST ORDER
	ret.port = ntohs(sin.sin_port);
	return OK;
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

char pop_i8(std::stringstream &ss)
{
	assert(sizeof(char) == 1);
	return ss.get();
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

void push_i8(std::stringstream &ss, char val)
{
	assert(sizeof(char) == 1);
	ss.put(val);
}

void push_i16(std::stringstream &ss, short val)
{
	assert(sizeof(short) == 2);
	val = htons(val);
	ss.write((char*)&val, sizeof(short));
}

void push_i64(std::stringstream &ss, long val)
{
	assert(sizeof(long) == 8);
	int num = 0x01;
	char *test = (char*)&num;

	if(*test == 0x01) // test the first byte
	{
		// little endian -- convert to big endian
		push_i8(ss, val >> 56);
		push_i8(ss, val >> 48);
		push_i8(ss, val >> 40);
		push_i8(ss, val >> 32);
		push_i8(ss, val >> 24);
		push_i8(ss, val >> 16);
		push_i8(ss, val >> 8);
		push_i8(ss, val);
	}
	else
	{
		ss.write((char*)&val, sizeof(long));
	}
}

void push_f32(std::stringstream &ss, float val)
{
	//TODO hope that it works...
	assert(sizeof(float) == 4);
	ss.write((char*)&val, sizeof(float));
}

void push_f64(std::stringstream &ss, double val)
{
	//TODO hope that it works...
	assert(sizeof(double) == 8);
	ss.write((char*)&val, sizeof(double));
}

long pop_i64(std::stringstream &ss)
{
	assert(sizeof(long) == 8);
	int num = 0x01;
	char *test = (char*)&num;
	long ret;
	char *casted = (char*)&ret;
	char buf[sizeof(long)];
	ss.read(buf, sizeof(long));

	if(*test == 0x01) // test the first byte
	{
		// little endian -- convert back to little endian
		casted[0] = buf[7];
		casted[1] = buf[6];
		casted[2] = buf[5];
		casted[3] = buf[4];
		casted[4] = buf[3];
		casted[5] = buf[2];
		casted[6] = buf[1];
		casted[7] = buf[0];
	}
	else
	{
		ss.read((char*)&ret, sizeof(long));
	}

	return ret;
}

short pop_i16(std::stringstream &ss)
{
	// assumes ss contains the valid bytes
	short ret;
	assert(sizeof(short) == 2);
	ss.read((char*)&ret, sizeof(short));
	return ntohs(ret);
}

float pop_f32(std::stringstream &ss)
{
	float ret;
	assert(sizeof(float) == 4);
	ss.read((char*)&ret, sizeof(float));
	return ret;
}

double pop_f64(std::stringstream &ss)
{
	// assumes ss contains the valid bytes
	double ret;
	assert(sizeof(double) == 8);
	ss.read((char*)&ret, sizeof(double));
	return ret;
}

ScopedLock::ScopedLock(pthread_mutex_t &mutex) : mutex(mutex)
{
	pthread_mutex_lock(&mutex);
}

ScopedLock::~ScopedLock()
{
	pthread_mutex_unlock(&mutex);
}
