#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#endif

// ============== class definitions  ==============

#if 0
struct RpcTrigger : public TCP::Sockets::Trigger
{
	virtual void when_connected(int fd);
	virtual void when_disconnected(int fd);
};
#endif

// ============== global variables ==============

class Global
{
public: //public

	// network structures
	NameService ns;
	//RpcTrigger trigger;
	TCP::Sockets sockets;
	Postman postman;

	// server (only set when the process is a server)
	int server_fd;

	// binder (used by both clients and servers)
	int binder_fd;

public: //methods
	Global()
		: postman(sockets, ns),
		  server_fd(-1),
		  binder_fd(-1)
	{
		// this constructor should not throw exception
		//this->sockets.set_trigger(&trigger);
		this->sockets.set_buffer(&postman);
	}
};

Global g;

// ============== codes below ==============

int rpcInit()
{
#ifndef NDEBUG
	std::cout << "RPC INIT" << std::endl;
#endif
	g.server_fd = g.sockets.bind_and_listen();

	if(g.server_fd < 0)
	{
		// should not happen in the student environment
		assert(false);
		return -1;
	}

	int port;
	std::string server_hostname;
	get_hostname(g.server_fd, server_hostname, port);
#ifndef NDEBUG
	print_host_info(g.server_fd,"SERVER");
#endif
	g.binder_fd = connect_to_binder(g.sockets);

	if(g.binder_fd < 0)
	{
		// cannot connect to the binder
		return -1;
	}

	if(g.postman.send_iam_server(g.binder_fd, port) < 0)
	{
		// should not happen...
		return -1;
	}

	Postman::Request req;
	Timer timer; // double-timer

	while(g.postman.sync_and_receive_any(req) < 0)
	{
		if(req.message.msg_type == Postman::OK_SERVER)
		{
			break;
		}

		// else must be broadcasts for new server nodes (which we don't care since we will get the latest logs)

		if(timer.is_timeout())
		{
			// should not happen
			return -1;
		}
	}

	std::stringstream ss(req.message.str);
	g.ns.apply_logs(ss);
	return 0;
}

int rpcCall(char* name, int* argTypes, void** args)
{
#ifndef NDEBUG
	std::cout << "RPC CALL" << std::endl;
#endif

	if(g.binder_fd < 0)
	{
		g.binder_fd = connect_to_binder(g.sockets);

		if(g.binder_fd < 0)
		{
			// cannot connect to the binder -- should not happen
			assert(false);
			return -1;
		}
	}

	Function func = to_function(name, argTypes);

	if(g.postman.send_loc_request(g.binder_fd, func) < 0)
	{
		// didn't send successfully
		return -1;
	}

	Postman::Request req;

	if(g.postman.sync_and_receive_any(req) < 0)
	{
		// didn't get any response/timeout
		return -1;
	}

	std::stringstream ss(req.message.str);

	switch(req.message.msg_type)
	{
		case Postman::LOC_SUCCESS:
		{
			unsigned target_id = pop_i32(ss);
			g.ns.apply_logs(ss);
			Name name;

			if(g.ns.resolve(target_id, name) < 0)
			{
				// cannot happen -- the database is synced with the binder
				assert(false);
				return -1;
			}

			int listen_port; // name.port is for sending msg, not listening

			if(g.ns.get_listen_port(target_id, listen_port) < 0)
			{
				// cannot happen -- the database is synced with the binder
				assert(false);
				return -1;
			}

			int target_fd = g.sockets.connect_remote(name.ip, listen_port);

			if(target_fd < 0)
			{
				// this can happen when the server (immediately) after the binder replies
				assert(false);
				return -1;
			}

			return g.postman.send_execute(target_fd, func, args);
		}

		case Postman::LOC_FAILURE:
		{
			Postman::ErrorNo err = static_cast<Postman::ErrorNo>(pop_i32(ss));

			switch(err)
			{
				case Postman::NO_AVAILABLE_SERVER:
					g.ns.apply_logs(ss);
					// do something?
					return -1;

				default:
					// not supposed to happen
					assert(false);
					return -1;
			}
		}

		default:
			// invalid command
			return -1;
	}

	// unreachable
	assert(false);
	return -1;
}

int rpcCacheCall(char* name, int* argTypes, void** args)
{
	(void)args;
	//TODO
	assert(false);
	if(g.binder_fd >= 0)
	{
		// cannot connect to the binder -- should not happen
		return -1;
	}

	Function func = to_function(name, argTypes);
	return -1;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
	//TODO make a local database of skeletons in the server
	(void)f;
#ifndef NDEBUG
	std::cout << "RPC REGISTER" << std::endl;
#endif
	// only the server calls this methods, and hello is sent during init()
	Function func = to_function(name, argTypes);

	if(g.postman.send_register(g.binder_fd, func) < 0)
	{
		// cannot send the request
		return -1;
	}

	Postman::Request req;

	if(g.postman.sync_and_receive_any(req) < 0 || req.message.msg_type != Postman::REGISTER_DONE)
	{
		// cannot get a reply or there is a bug (someone sent an invalid reply)
		return -1;
	}

	// reply contains nothing but log deltas
	std::stringstream ss(req.message.str);
	g.ns.apply_logs(ss);
	return 0;
}

int rpcExecute()
{
#ifndef NDEBUG
	std::cout << "RPC EXECUTE" << std::endl;
#endif

	// only the server calls this methods, and hello is sent during init()
	while(true)
	{
		Postman::Request req;

		if(g.postman.sync_and_receive_any(req) >= 0)
		{
			//TODO
#ifndef NDEBUG
			std::cout << "got something in rpcExecute()" << std::endl;
#endif
		}
	}

	return -1;
}

int rpcTerminate()
{
#ifndef NDEBUG
	std::cout << "RPC TERMINATE" << std::endl;
#endif
	return -1;
}

#if 0
void RpcTrigger::when_connected(int fd)
{
#ifndef NDEBUG
	std::cout << "connection created for fd:" << fd << std::endl;
#endif
}

void RpcTrigger::when_disconnected(int fd)
{
#ifndef NDEBUG
	std::cout << "connection ended for fd:" << fd << std::endl;
#endif
}
#endif
