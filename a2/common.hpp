#ifndef _common_hpp_
#define _common_hpp_

#include <deque>
#include <string>
#include <map>
#include <set>
#include <sys/select.h>

namespace TCP
{

class Socket
{
	typedef std::deque<unsigned char> Message;
	typedef std::map<int, Message> Requests;

	Requests read_buf, write_buf;
	int local_fd;
	std::set<int> connected_fds;

	// avoid reallocation
	fd_set readfds, writefds;

private: // functions
	int get_max_fd() const;
	void setup_read_fds(fd_set &fds);
	void setup_write_fds(fd_set &fds);
	int create_socket();

public:
	Socket();
	~Socket();
	int bind_and_listen(int port = 0, int num_listen = 100);
	int connect_remote(char *ip, int port);
	int get_message(int dst_fd, int &ret);
	int push_message(int dst_fd, int c);
	int flush_write(int dst_fd);
	int sync();
	void get_name(std::string *ipv4, int *port) const;
};

}

#endif
