#ifndef _postman_hpp_
#define _postman_hpp_

#include "common.hpp"
#include "sockets.hpp"
#include <map>
#include <queue>
#include <string>
#include <pthread.h>

struct Function;
class NameService;

/*
	This class is responsible for sending and receiving messages from the sockets.
	All public methods are synchronous. Note: each buffer has its own mutex.
*/
class Postman : public TCP::Sockets::DataBuffer
{
public: // typedefs
	// these are flags
	enum MessageType
	{
		ASK_NS_UPDATE       = (1 <<  0),
		NS_UPDATE_SENT      = (1 <<  1), // generic reply to "void" requests
		I_AM_SERVER         = (1 <<  2), // send the port that listen for requests
		SERVER_OK           = (1 <<  3), // generic reply to "void" requests
		REGISTER            = (1 <<  4),
		REGISTER_DONE       = (1 <<  5),
		LOC_REQUEST         = (1 <<  6),
		LOC_REPLY           = (1 <<  7),
		EXECUTE             = (1 <<  8),
		EXECUTE_REPLY       = (1 << 10),
		CONFIRM_TERMINATE   = (1 << 11), // server ask this question to the binder
		NEW_SERVER_EXECUTE  = (1 << 12),
		TERMINATE           = (1 << 13)
	};
	struct Message
	{
		unsigned int ns_version;
		unsigned int size;
		MessageType msg_type;
		std::string str;
	};
	struct Request
	{
		int fd;
		Message message;
	};
	typedef std::queue<Request> IncomingRequests;
	typedef std::map<int, std::queue<Message> > OutgoingRequests;
	typedef std::map<int, Message> AssembleBuffer;
private: // data
	TCP::Sockets sockets;
	IncomingRequests incoming;
	OutgoingRequests outgoing;
	AssembleBuffer asm_buf;
	pthread_mutex_t incoming_mutex, outgoing_mutex, asm_buf_mutex, soc_mutex;

public: // refernces
	NameService &ns;

private: // helper methods
	Message to_message(MessageType type, std::string msg);
	int send(int remote_fd, Message &msg);

	// this is for polling only -- need to call sync() separately
	int receive_any(Request &ret);

public: // methods
	Postman(NameService &ns);
	~Postman();

	// forward to Sockets
	int bind_and_listen(int port = 0, int num_listen = 100);
	size_t is_alive(int fd);
	int connect_remote(const char *hostname, int port);
	int connect_remote(int ip, int port);
	void disconnect(int fd);

	// send requests
	int send_confirm_terminate(int remote_fd, bool is_terminate = true);
	int send_execute(int server_fd, const Function &func, void **args, bool is_force_queue_task);
	int send_iam_server(int binder_fd, int listen_port);
	int send_loc_request(int binder_fd, const Function &func);
	int send_new_server_execute(int remote_fd);
	int send_ns_update(int remote_fd);
	int send_register(int binder_fd, int my_id, const Function &func);
	int send_terminate(int remote_fd);

	// send replies
	int reply_execute(int remote_fd, int retval, const Function &func, void **args, unsigned remote_ns_version);
	int reply_loc_request(int remote_fd, const Function &func, unsigned remote_ns_version);
	int reply_ns_update(int remote_fd, unsigned remote_ns_version);
	int reply_register(int remote_fd, unsigned remote_ns_version);
	int reply_server_ok(int remote_fd, unsigned id, unsigned remote_ns_version);
	int reply_update_ns(int remote_fd, unsigned remote_ns_version);

	// this is a blockying (busy-wait) method
	int sync_and_receive_any(Request &ret, int *need_alive_fd = NULL);

	// defined by TCP::Sockets::DataBuffer
	virtual void read_avail(int fd, const std::string &got);
	virtual const std::string write_avail(int fd);
};

// since there are many instances where calls can fail,
// this class takes advantage of RAII to auto disconnect
// when the connect is out of scope
class ScopedConnection
{
public: // data
	int fd;
	Postman &postman;
public: // helper methods
	ScopedConnection(Postman &postman, int ip, int port);
	ScopedConnection(Postman &postman, const char *hostname, int port);
	~ScopedConnection();
	int get_fd() const;
};

#endif
