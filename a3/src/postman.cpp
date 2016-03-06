#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include <cassert>
#include <sstream>

static void push(std::stringstream &ss, Postman::Message &msg);

Postman::Postman(NameService &ns) :
	ns(ns)
{
	int retval = pthread_mutex_init(&this->asm_buf_mutex, NULL);
	(void) retval;
	assert(retval == 0);
	retval = pthread_mutex_init(&this->soc_mutex, NULL);
	assert(retval == 0);
	retval = pthread_mutex_init(&this->incoming_mutex, NULL);
	assert(retval == 0);
	retval = pthread_mutex_init(&this->outgoing_mutex, NULL);
	assert(retval == 0);
	this->sockets.set_buffer(this);
}

Postman::~Postman()
{
	pthread_mutex_destroy(&this->asm_buf_mutex);
	pthread_mutex_destroy(&this->soc_mutex);
	pthread_mutex_destroy(&this->incoming_mutex);
	pthread_mutex_destroy(&this->outgoing_mutex);
}

int Postman::send(int remote_fd, Message &msg)
{
	{
		ScopedLock lock(this->outgoing_mutex);
		this->outgoing[remote_fd].push(msg);
	}
	ScopedLock lock(this->soc_mutex);
	return this->sockets.flush(remote_fd);
}

int Postman::send_register(int binder_fd, int my_id, const Function &func)
{
	std::stringstream ss;
	push_i32(ss, my_id);
	push(ss, func);
	Message msg = to_message(REGISTER, ss.str());
	return this->send(binder_fd, msg);
}

int Postman::send_execute(int server_fd, const Function &func, void **args)
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
		size_t cardinality = get_arg_car(arg_type);

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
				float *argsi = (float*)args[i];

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

int Postman::reply_execute(int remote_fd, int retval_got, const Function &func, void **args, unsigned remote_ns_version)
{
	std::stringstream ss;
	push_i32(ss, retval_got);

	// extract output
	for(size_t i = 0; i < func.types.size(); i++)
	{
		int arg_type = func.types[i];
		void * const argsi = args[i];
		size_t cardinality = get_arg_car(arg_type);

		if(!is_arg_output(arg_type))
		{
			continue;
		}

		switch(get_arg_data_type(arg_type))
		{
			case ARG_CHAR:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_i8(ss, ((char*)argsi)[j]);
				}

				break;

			case ARG_SHORT:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_i16(ss, ((short*)args[i])[j]);
				}

				break;

			case ARG_INT:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_i32(ss, ((int*)args[i])[j]);
				}

				break;

			case ARG_LONG:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_i64(ss, ((long*)argsi)[j]);
				}

				break;

			case ARG_DOUBLE:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_f64(ss, ((double*)args[i])[j]);
				}

				break;

			case ARG_FLOAT:
				for(size_t j = 0; j < cardinality; j++)
				{
					push_f32(ss, ((float*)args[i])[j]);
				}

				break;
		}
	}

	ss << this->ns.get_logs(remote_ns_version);
	Message msg = to_message(EXECUTE_REPLY, ss.str());
	return this->send(remote_fd, msg);
}

int Postman::send_ns_update(int remote_fd)
{
	Message msg = to_message(ASK_NS_UPDATE, "");
	return this->send(remote_fd, msg);
}

int Postman::send_terminate(int remote_fd)
{
	Message msg = to_message(TERMINATE, "");
	return this->send(remote_fd, msg);
}

int Postman::send_confirm_terminate(int remote_fd, bool is_terminate)
{
	std::stringstream ss;
	push_i8(ss, is_terminate);
	Message msg = to_message(CONFIRM_TERMINATE, ss.str());
	return this->send(remote_fd, msg);
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

int Postman::sync_and_receive_any(Request &ret, int *need_alive_fd, int timeout)
{
	Timer timer(timeout);

	// busy-wait until "good to see you" reply is back
	while(this->receive_any(ret) < 0)
	{
		ScopedLock lock(this->soc_mutex);

		if(need_alive_fd != NULL && !this->sockets.is_alive(*need_alive_fd))
		{
			// need-alive target has been disconnected, so error
			return REMOTE_DISCONNECTED;
		}

		if(this->sockets.sync() < 0)
		{
			// some error occurred
			//TODO ???
			assert(false);
		}

		if(timer.is_timeout())
		{
			return TIMEOUT;
		}
	}

	return OK;
}

int Postman::send_loc_request(int binder_fd, const Function &func)
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

int Postman::reply_ns_update(int remote_fd, unsigned remote_ns_version)
{
	std::stringstream ss;
	ss << this->ns.get_logs(remote_ns_version);
	Message msg = to_message(NS_UPDATE_SENT, ss.str());
	return send(remote_fd, msg);
}

int Postman::reply_server_ok(int remote_fd, unsigned id, unsigned remote_ns_version)
{
	std::stringstream ss;
	push_i32(ss, id);
	ss << this->ns.get_logs(remote_ns_version);
	Message msg = to_message(SERVER_OK, ss.str());
	return send(remote_fd, msg);
}

int Postman::reply_loc_request(int remote_fd, const Function &func, unsigned remote_ns_version)
{
	unsigned target_id;
	std::stringstream ss;
	Message msg;
	bool is_binder = true; // this method can only be called by the binder

	if(this->ns.suggest(*this, func, target_id, is_binder) < 0)
	{
		push_i8(ss, false); // failure
		ss << this->ns.get_logs(remote_ns_version);
		// cannot find any server (no suggestion)
		push_i32(ss, NO_AVAILABLE_SERVER);
	}
	else
	{
		push_i8(ss, true); // success
		ss << this->ns.get_logs(remote_ns_version);
		// got a suggestion (with round-robin)
		push_i32(ss, target_id);
	}

	msg = to_message(LOC_REPLY, ss.str());
	return send(remote_fd, msg);
}

void Postman::read_avail(int fd, const std::string &got)
{
	std::stringstream ss(got);
	ScopedLock lock(this->asm_buf_mutex);
	Message &req_buf = this->asm_buf[fd];

	if(got.empty())
	{
		assert(false);
		return;
	}

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
	delete [] buf;

	if(num_read == remaining)
	{
		// this request is ready
		Request req = { fd, req_buf }; // copy message
		{
			ScopedLock(this->incoming_mutex);
			incoming.push(req);
		}
		// remove the temporary object in the assembler
		this->asm_buf.erase(fd);
	}
}

int Postman::receive_any(Request &ret)
{
	ScopedLock lock(this->incoming_mutex);

	if(this->incoming.empty())
	{
		return -1; // internal use only
	}

	// copy and remove
	ret = this->incoming.front();
	this->incoming.pop();
	return OK;
}

const std::string Postman::write_avail(int fd)
{
	std::stringstream ss;
	{
		ScopedLock lock(this->outgoing_mutex);
		std::queue<Message> &requests = this->outgoing[fd];

		while(!requests.empty())
		{
			push(ss, requests.front());
			requests.pop();
		}
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

int Postman::connect_remote(const char *hostname, int port)
{
	ScopedLock lock(this->soc_mutex);
	return this->sockets.connect_remote(hostname, port);
}

int Postman::connect_remote(int ip, int port)
{
	ScopedLock lock(this->soc_mutex);
	return this->sockets.connect_remote(ip, port);
}

int Postman::bind_and_listen(int port, int num_listen)
{
	ScopedLock lock(this->soc_mutex);
	return this->sockets.bind_and_listen(port, num_listen);
}

void Postman::disconnect(int fd)
{
	ScopedLock lock(this->soc_mutex);
	this->sockets.disconnect(fd);
}

ScopedConnection::ScopedConnection(Postman &postman, int ip, int port) : fd(postman.connect_remote(ip, port)), postman(postman) {}

ScopedConnection::ScopedConnection(Postman &postman, const char *hostname, int port) : fd(postman.connect_remote(hostname, port)), postman(postman) {}

ScopedConnection::~ScopedConnection()
{
	postman.disconnect(fd); // fd can be -1; Sockets ignore bad fds
}

int ScopedConnection::get_fd() const
{
	return fd;
}

size_t Postman::is_alive(int fd)
{
	ScopedLock lock(this->soc_mutex);
	return this->sockets.is_alive(fd);
}

int Postman::send_new_server_execute(int binder_fd)
{
	Message msg = to_message(NEW_SERVER_EXECUTE, "");
	return this->send(binder_fd, msg);
}
