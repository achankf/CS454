#ifndef _postman_hpp_
#define _postman_hpp_

#include "common.hpp"
#include "sockets.hpp"
#include <map>
#include <queue>
#include <string>

struct Function;
class NameService;

class Postman : public TCP::Sockets::DataBuffer
{
public: // typedefs
	enum ErrorNo
	{
		NO_AVAILABLE_SERVER
	};

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
		EXECUTE_REPLY       = (1 <<  9),
		CONFIRM_TERMINATE   = (1 << 10), // server ask this question to the binder
		TERMINATE           = (1 << 11)
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
	IncomingRequests incoming;
	OutgoingRequests outgoing;
	AssembleBuffer asmBuf;

public: // refernces
	TCP::Sockets &sockets;
	NameService &ns;

private: // helper methods
	Message to_message(MessageType type, std::string msg);
	int send(int remote_fd, Message &msg);

	// this is for polling only -- need to call sync() separately
	int receive_any(Request &ret);

public: // methods
	Postman(TCP::Sockets &sockets, NameService &ns);

	// send requests
	int send_execute(int server_fd, Function &func, void **args);
	int send_loc_request(int binder_fd, Function &func);
	int send_ns_update(int remote_fd);
	int send_register(int binder_fd, int my_id, Function &func);
	int send_terminate(int remote_fd);
	int send_iam_server(int binder_fd, int listen_port);
	int send_confirm_terminate(int remote_fd, bool is_terminate = true);

	// send replies
	int reply_execute(int remote_fd, unsigned remote_ns_version);
	int reply_loc_request(int remote_fd, Function &func, unsigned remote_ns_version);
	int reply_register(int remote_fd, unsigned remote_ns_version);
	int reply_update_ns(int remote_fd, unsigned remote_ns_version);
	int reply_ns_update(int remote_fd, unsigned remote_ns_version);
	int reply_server_ok(int remote_fd, unsigned id, unsigned remote_ns_version);

	// this is a blockying (busy-wait) method
	int sync_and_receive_any(Request &ret, int timeout = DEFAULT_TIMEOUT);

	int sync();

	// defined by TCP::Sockets::DataBuffer
	virtual void read_avail(int fd, const std::string &got);
	virtual const std::string write_avail(int fd);
};

#endif
