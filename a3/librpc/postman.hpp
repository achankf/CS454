#ifndef _postman_hpp_
#define _postman_hpp_

#include "sockets.hpp"
#include <map>
#include <queue>
#include <string>

struct Function;
class NameService;

class Postman
{
public: // typedefs
	enum MessageType
	{
		HELLO,
		GOOD_TO_SEE_YOU,
		UPDATE_NS,
		UPDATE_NS_SENT,
		REGISTER,
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
		std::string message;
	};
	struct Request
	{
		int fd;
		Message message;
	};
	typedef std::queue<Request> Requests;
	typedef std::map<int,Message> AssembleBuffer;
private: // data
	TCP::Sockets &sockets;
	NameService &ns;
	Requests incoming, outgoing;
	Requests reqs;
	AssembleBuffer asmBuf;
private: // methods
	int send(int remote_fd, Message &msg);
public: // methods
	Postman(TCP::Sockets &sockets, NameService &ns);

	int send_hello(int binder_fd);
	int send_register(int binder_fd, int server_id, int port, Function &func);
	int send_call(int server_fd, Function &func, void **args);
	int send_ns_update(int remote_fd);
	int send_terminate(int binder_fd);

	int reply_hello(int remote_fd);

	int receive_any(Request &ret);

	const TCP::Sockets::Fds &all_connected() const;
	int sync();

	Message to_message(MessageType type, std::string msg);
};

#endif
