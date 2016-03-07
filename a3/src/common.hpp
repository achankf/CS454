#ifndef _common_hpp_
#define _common_hpp_

#include "config.hpp"
#include <pthread.h>
#include <sstream>
#include <string>

// This file provides utility classes/methods.

struct Name;

enum ErrorNo
{
	EXECUTE_WITHOUT_REGISTER    =    2,
	SKELETON_UPDATED            =    1,
	OK                          =    0,
	BAD_FD                      =   -1,
	BINDER_UNAVAILABLE          =   -2,
	CANNOT_ACCEPT_CONNECTION    =   -3,
	CANNOT_BIND_PORT            =   -4,
	CANNOT_CONNECT_TO_SERVER    =   -5,
	CANNOT_LISTEN_PORT          =   -6,
	CANNOT_RESOLVE_HOSTNAME     =   -7,
	CANNOT_START_CONNECTION     =   -8,
	CANNOT_WRITE_TO_SOCKET      =   -9,
	FUNCTION_ARGS_ARE_INVALID   =  -10,
	FUNCTION_NAME_IS_INVALID    =  -11,
	FUNCTION_NOT_REGISTERED     =  -12,
	HAS_ALREADY_INIT_SERVER     =  -13,
	HAS_RUN_EXECUTE             =  -14,
	NOTHING_TO_RECEIVE          =  -15,
	NOTHING_TO_SEND             =  -16,
	NOT_A_CLIENT                =  -17,
	NOT_A_SERVER                =  -18,
	NO_AVAILABLE_SERVER         =  -19,
	REMOTE_DISCONNECTED         =  -20,
	SKELETON_FAILURE            =  -21,
	SKELETON_IS_NULL            =  -22,
	SERVER_HAS_NO_AVAIL_THREADS =  -23,
	TERMINATING                 =  -24,
	TIMEOUT                     =  -25,
	UNREACHABLE                 = -100
};

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
