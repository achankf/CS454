#ifndef _common_hpp_
#define _common_hpp_

#include "config.hpp"
#include <pthread.h>
#include <sstream>
#include <string>

struct Name;

class Timer
{
private: // data
	clock_t start;
	int timeout_in_seconds;
public: // methods
	Timer(int timeout_in_seconds = DEFAULT_TIMEOUT);
	bool is_timeout() const;
};

class ScopedLock
{
private: // references
	pthread_mutex_t &mutex;
public:
	ScopedLock(pthread_mutex_t &mutex);
	~ScopedLock();
};

// get the hostname and port # for the SERVER ONLY
// note: they don't check for errors
void get_hostname(int fd, std::string &hostname, int &port);
void print_host_info(int fd, const char *prefix);

int get_peer_info(int fd, Name &ret);

// buffer-related helpers
char pop_i8(std::stringstream &ss);
int pop_i32(std::stringstream &ss);
long pop_i64(std::stringstream &ss);
short pop_i16(std::stringstream &ss);
float pop_f32(std::stringstream &ss);
double pop_f64(std::stringstream &ss);
std::string pop_string(std::stringstream &ss);
std::string raw_read(std::stringstream &ss, size_t size);
void push(std::stringstream &ss, const std::string &str);
void push_f32(std::stringstream &ss, float val);
void push_f64(std::stringstream &ss, double val);
void push_i16(std::stringstream &ss, short val);
void push_i32(std::stringstream &ss, int val);
void push_i64(std::stringstream &ss, long val);
void push_i8(std::stringstream &ss, char val);

#endif
