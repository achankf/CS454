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
	notice that StringChannel assumes Message.ns_version, Message.size,
	and Message.msg_type to be read at once, so SOCKET_BUF_SIZE
	must be at least 12
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

	struct DataBuffer
	{
	public: // interface
		virtual ~DataBuffer() {}
		virtual void read_avail(int fd, const std::string &got) = 0;
		virtual const std::string write_avail(int fd) = 0;
	};

	typedef std::set<int> Fds;

private: // data
	int local_fd;
	Fds connected_fds;
	DataBuffer *buffer;

private: // functions

	// get the max fd for select()
	int get_max_fd() const;

	// each time sync() is called, fd_set's are repopulated with these 2 methods
	void setup_read_fds(fd_set &fds) const;
	void setup_write_fds(fd_set &fds) const;

	// used by connect_remote and bind_and_listen
	int create_socket();

	// unsynchronized version of the public methods
	int flush_helper(int dst_fd);

	size_t num_connected(int *exclude_fd = NULL) const;

public:
	Sockets();
	~Sockets();

	bool is_alive(int fd) const;
	void disconnect(int fd);

	// this is important -- if the buffer is not set, every incoming messages will be discarded
	void set_buffer(DataBuffer *buffer);

	// used by the server
	int bind_and_listen(int port = 0, int num_listen = 100);

	// used by the client to connect to the server
	int connect_remote(const char *hostname, int port);
	int connect_remote(int ip, int port);

	// flush the write buffer directly, BLOCKING (instead of via sync())
	int flush(int dst_fd);

	// send all requests in the write buffer to remote,
	// and read all incoming messages to the read buffer
	int sync();
};

}

#endif
