#ifndef _common_hpp_
#define _common_hpp_

#include <sstream>
#include <string>

#define DEBUG_PORT 42391

namespace TCP
{
class Sockets;
}

struct Name;

// get the hostname and port # for the SERVER ONLY
// note: they don't check for errors
void get_hostname(int fd, std::string &hostname, Name &name);
void print_host_info(int fd, const char *prefix);

int get_peer_info(int fd, Name &ret);

int connect_to_binder(TCP::Sockets &sockets);

std::stringstream &push_u32(std::stringstream &ss, unsigned int val);
unsigned int pop_uint32(std::stringstream &ss);
std::string get_unformatted(std::stringstream &ss, size_t size);

#endif
