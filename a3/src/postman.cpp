#include "name_service.hpp"
#include "postman.hpp"
#include "sockets.hpp"
#include <algorithm> // copy
#include <arpa/inet.h> // integer conversion
#include <cassert>
#include <iostream>
#include <sstream>

Postman::Postman(TCP::Sockets &sockets, NameService &ns) :
	sockets(sockets),
	ns(ns)
{
}

int Postman::send(int remote_fd, Message &msg)
{
	TCP::Sockets::Buffer &buf = this->sockets.get_write_buf(remote_fd);
	push_uint(buf, msg.ns_version);
	push_uint(buf, msg.msg_type);
	push_uint(buf, msg.size);
	const char *msg_cstr = msg.str.c_str();
	std::copy(msg_cstr, (msg_cstr + msg.size), std::inserter(buf, buf.end()));
	return this->sockets.flush(remote_fd);
}

int Postman::send_register(int binder_fd, int server_id, int port, Function &func)
{
	std::stringstream buf;
	buf << htonl(server_id)
	    << htonl(port)
	    << to_net_string(func);
	Message msg = to_message(REGISTER, buf.str());
	return this->send(binder_fd, msg);
}

int Postman::send_hello(int binder_fd)
{
	Message msg = to_message(HELLO, "");
	return this->send(binder_fd, msg);
}

int Postman::send_call(int server_fd, Function &func, void **args)
{
	return -1;
}

int Postman::send_ns_update(int remote_fd)
{
	return -1;
}

int Postman::send_terminate(int binder_fd)
{
	return -1;
}

int Postman::receive_any(Request &ret)
{
	if(this->sockets.sync() < 0)
	{
		// some error occurred
		//TODO ???
		assert(false);
		return -1;
	}

	std::pair<int, TCP::Sockets::Buffer*> raw_req;

	if(this->sockets.get_available_msg(raw_req) < 0)
	{
		// nothing available yet
		return -1;
	}

	int fd = raw_req.first;
	TCP::Sockets::Buffer &read_buf = *raw_req.second;
	assert(read_buf.size() > 0);
	Message &req_buf = this->asmBuf[fd];

	if(req_buf.str.empty())
	{
		// this is a new request, so the header (8 bytes) must be read
		pop_uint(read_buf, req_buf.ns_version);
		pop_uint(read_buf, (unsigned int &)req_buf.msg_type);
		pop_uint(read_buf, req_buf.size);
	}

	assert(req_buf.size > 0);
	assert(req_buf.msg_type >= 0 && req_buf.msg_type <= TERMINATE);
	unsigned int size_minus_one = req_buf.size - 1;

	while(!read_buf.empty())
	{
		unsigned char c = read_buf.front();
		read_buf.pop_front();

		if(req_buf.str.size() == size_minus_one)
		{
			int retval = -1;

			if(c == '\0')
			{
				// whole message read
				Request temp = {fd, req_buf};
				ret = temp;
				retval = 0;
			}

			// else: message is corrupted?
			// remove the finished request from the buffer
			this->asmBuf.erase(fd);
			return retval;
		}

		req_buf.str.push_back(c);
	}

	return -1;
}

int Postman::sync()
{
	return this->sockets.sync();
}

Postman::Message Postman::to_message(Postman::MessageType type, std::string msg)
{
	Postman::Message ret = {this->ns.get_version(), msg.size() + 1, type, msg};
	return ret;
}

const TCP::Sockets::Fds &Postman::all_connected() const
{
	return this->sockets.all_connected();
}

#include <iomanip>
int Postman::reply_hello(int remote_fd, unsigned log_since)
{
#ifndef NDEBUG
	std::cout << "replying hello to fd:" << remote_fd << std::endl;
#endif
	std::string delta_logs = ns.get_logs(log_since);
	Message msg = to_message(GOOD_TO_SEE_YOU, delta_logs);
	std::cout << std::setfill('0');

	for(size_t i = 0; i < msg.str.size(); i++)
	{
		std::cout << " " << std::hex << std::setw(2) << ((unsigned)msg.str[i] &0xff);
	}

	std::cout << std::endl << std::dec;
	return send(remote_fd, msg);
}
