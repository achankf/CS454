#ifndef _postman_hpp_
#define _postman_hpp_

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
	enum MessageType
	{
		I_AM_SERVER, // send the port that listen for requests
		OK_SERVER, // reply for I_AM_SERVER
		REGISTER,
		REGISTER_DONE,
		LOC_REQUEST,
		LOC_SUCCESS,
		LOC_FAILURE,
		EXECUTE,
		EXECUTE_SUCCESS,
		EXECUTE_FAILURE,
		TERMINATE
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
	TCP::Sockets &sockets;
	NameService &ns;
	IncomingRequests incoming;
	OutgoingRequests outgoing;
	AssembleBuffer asmBuf;

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
	int send_register(int binder_fd, Function &func);
	int send_terminate(int binder_fd);
	int send_iam_server(int binder_fd, int listen_port);

	// send replies
	int broadcast_terminate();
	int reply_execute(int remote_fd, unsigned remote_ns_version);
	int reply_loc_request(int remote_fd, Function &func, unsigned remote_ns_version);
	int reply_register(int remote_fd, unsigned remote_ns_version);
	int reply_update_ns(int remote_fd, unsigned remote_ns_version);
	int reply_iam_server(int remote_fd, unsigned remote_ns_version);

	// this is a blockying (busy-wait) method
	int sync_and_receive_any(Request &ret, int timeout = 60);

	int sync();

	// defined by TCP::Sockets::DataBuffer
	virtual void read_avail(int fd, const std::string &got);
	virtual const std::string write_avail(int fd);
};

#endif
