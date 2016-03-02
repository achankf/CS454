#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "sockets.hpp"
#include <algorithm> // copy
#include <arpa/inet.h> // integer conversion
#include <cassert>
#include <iostream>
#include <sstream>

static void push(std::stringstream &ss, Postman::Message &msg);

Postman::Postman(TCP::Sockets &sockets, NameService &ns) :
	sockets(sockets),
	ns(ns)
{
}

int Postman::send(int remote_fd, Message &msg)
{
	this->outgoing[remote_fd].push(msg);
	return this->sockets.flush(remote_fd);
}

int Postman::send_register(int binder_fd, Function &func)
{
	std::stringstream ss;
	push(ss, func);
	Message msg = to_message(REGISTER, ss.str());
	return this->send(binder_fd, msg);
}

int Postman::send_execute(int server_fd, Function &func, void **args)
{
	std::stringstream ss;
	push(ss, func);

	for(size_t i = 0; i < func.types.size(); i++)
	{
		int arg_type = func.types[i];

		if(!is_arg_input(arg_type))
		{
			continue;
		}

		// there is no point in differentiating b/w scalar and array of size 1
		size_t cardinality = std::max(1lu, get_arg_car(arg_type));
		push_i16(ss, cardinality);

		switch(get_arg_data_type(arg_type))
		{
			case ARG_CHAR:
			{
				char *argsi = (char*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_i8(ss, argsi[j]);
				}

				break;
			}

			case ARG_SHORT:
			{
				short *argsi = (short*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_i16(ss, argsi[j]);
				}

				break;
			}

			case ARG_INT:
			{
				int *argsi = (int*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_i32(ss, argsi[j]);
				}

				break;
			}

			case ARG_LONG:
			{
				long *argsi = (long*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_i64(ss, argsi[j]);
				}

				break;
			}

			case ARG_DOUBLE:
			{
				double *argsi = (double*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_f64(ss, argsi[j]);
				}

				break;
			}

			case ARG_FLOAT:
			{
				double *argsi = (double*)args[i];

				for(size_t j = 0; j < cardinality; j++)
				{
					push_f32(ss, argsi[j]);
				}

				break;
			}
		}
	}

	Message msg = to_message(EXECUTE, ss.str());
	return this->send(server_fd, msg);
}

int Postman::send_ns_update(int remote_fd)
{
	(void) remote_fd;
	assert(false);
	return -1;
}

int Postman::send_terminate(int binder_fd)
{
	(void) binder_fd;
	assert(false);
	return -1;
}

int Postman::sync()
{
	return this->sockets.sync();
}

Postman::Message Postman::to_message(Postman::MessageType type, std::string msg)
{
	Postman::Message ret = {this->ns.get_version(), msg.size(), type, msg};
	return ret;
}

int Postman::reply_register(int remote_fd, unsigned remote_ns_version)
{
	Message msg = to_message(REGISTER_DONE, this->ns.get_logs(remote_ns_version));
	return send(remote_fd, msg);
}

int Postman::sync_and_receive_any(Request &ret, int timeout)
{
	Timer timer(timeout);

	// busy-wait until "good to see you" reply is back
	while(this->receive_any(ret) < 0)
	{
		if(this->sync() < 0)
		{
			// some error occurred
			//TODO ???
			assert(false);
		}

		if(timer.is_timeout())
		{
			// probably not likely going to happen
#ifndef NDEBUG
			std::cout << "timeout" << std::endl;
#endif
			return -1;
		}
	}

	return 0;
}

int Postman::send_loc_request(int binder_fd, Function &func)
{
	std::stringstream ss;
	push(ss, func);
	Message msg = to_message(LOC_REQUEST, ss.str());
	return send(binder_fd, msg);
}

int Postman::send_iam_server(int binder_fd, int listen_port)
{
	std::stringstream ss;
	push_i32(ss, listen_port);
	Message msg = to_message(I_AM_SERVER, ss.str());
	return send(binder_fd, msg);
}

int Postman::reply_iam_server(int remote_fd, unsigned remote_ns_version)
{
	std::stringstream ss;
	ss << this->ns.get_logs(remote_ns_version);
	Message msg = to_message(OK_SERVER, ss.str());
	return send(remote_fd, msg);
}

int Postman::reply_loc_request(int remote_fd, Function &func, unsigned remote_ns_version)
{
	unsigned target_id;
	std::stringstream ss;
	Message msg;

	if(this->ns.suggest(func, target_id) < 0)
	{
		// cannot find any server (no suggestion)
		push_i32(ss, NO_AVAILABLE_SERVER);
		// sync remote's log anyway
		ss << this->ns.get_logs(remote_ns_version);
		msg = to_message(LOC_FAILURE, ss.str());
	}
	else
	{
		// got a suggestion (with round-robin)
		push_i32(ss, target_id);
		ss << this->ns.get_logs(remote_ns_version);
		msg = to_message(LOC_SUCCESS, ss.str());
	}

	return send(remote_fd, msg);
}

void Postman::read_avail(int fd, const std::string &got)
{
	std::stringstream ss(got);
	Message &req_buf = this->asmBuf[fd];

	if(req_buf.str.empty())
	{
		// this is a new request, so the header (8 bytes) must be read
		req_buf.ns_version = pop_i32(ss);
		req_buf.msg_type = static_cast<MessageType>(pop_i32(ss));
		req_buf.size = pop_i32(ss);
		// data + null character
		req_buf.str.reserve(req_buf.size + 1);
	}

	// copy message into the assembling buffer
	size_t remaining = req_buf.size - req_buf.str.size();
	char *buf = new char[remaining];
	ss.read(buf, remaining);
	size_t num_read = static_cast<size_t>(ss.gcount());
	req_buf.str.append(buf, num_read);
	delete buf;

	if(num_read == remaining)
	{
		// this request is ready
		Request req = { fd, req_buf }; // copy message
		incoming.push(req);
		// remove the temporary object in the assembler
		this->asmBuf.erase(fd);
	}
}

int Postman::receive_any(Request &ret)
{
	if(this->incoming.empty())
	{
		return -1;
	}

	// copy and remove
	ret = this->incoming.front();
	this->incoming.pop();
	return 0;
}

const std::string Postman::write_avail(int fd)
{
	std::stringstream ss;
	std::queue<Message> &requests = this->outgoing[fd];

	while(!requests.empty())
	{
		push(ss, requests.front());
		requests.pop();
	}

	return ss.str();
}

static void push(std::stringstream &ss, Postman::Message &msg)
{
	push_i32(ss, msg.ns_version);
	push_i32(ss, msg.msg_type);
	push_i32(ss, msg.size);
	ss.write(msg.str.c_str(), msg.str.size());
}
