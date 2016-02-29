#ifndef _sockets_hpp_
#define _sockets_hpp_

#include <deque>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <sys/select.h>

/*
	this buffer should be large enough to prevent segfaults
	notice that StringChannel assumes all 4 bytes for the string size
	to be read at once, so SOCKET_BUF_SIZE must be at least 5
*/
#define SOCKET_BUF_SIZE 2048

/*
	Conventions
		- methods that create sockets usually returns the fd
		- methods that return int usually return 2 types of value
			- negative values mean failure
			- non-negative values mean succuess
*/
namespace TCP
{

class Sockets
{
public: // typedefs
	typedef std::deque<unsigned char> Buffer;
	typedef std::map<int, Buffer> Requests;
	typedef std::set<int> Fds;

private: // data
	Requests read_buf, write_buf;
	int local_fd;
	Fds connected_fds;

private: // functions

	// get the max fd for select()
	int get_max_fd() const;

	// each time sync() is called, fd_set's are repopulated with these 2 methods
	void setup_read_fds(fd_set &fds) const;
	void setup_write_fds(fd_set &fds) const;

	// used by connect_remote and bind_and_listen
	int create_socket();

	// IMPORTANT: this is not idempotent
	void disconnect(int fd);

public:
	Sockets();
	~Sockets();

	// used by the server
	int bind_and_listen(int port = 0, int num_listen = 100);

	// used by the client to connect to the server
	int connect_remote(char *hostname, int port);

	// getters for the buffer; IMPORTANT: not synchronized
	Buffer &get_read_buf(int dst_fd);
	Buffer &get_write_buf(int dst_fd);
	int get_available_msg(std::pair<int,Buffer*> &ret);

	// flush the write buffer directly, BLOCKING (instead of via sync())
	int flush(int dst_fd);

	const Fds &all_connected() const;

	// send all requests in the write buffer to remote,
	// and read all incoming messages to the read buffer
	int sync();
};

}

// helpers for manipulating the buffers
void push_uint(TCP::Sockets::Buffer &buf, unsigned int val);
int pop_uint(TCP::Sockets::Buffer &buf, unsigned int &val);

#endif
