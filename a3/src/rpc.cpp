#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include <cassert>
#include <cstdlib>
#include <deque>
#include <map>

#ifndef NDEBUG
#include <iostream>
#endif

// ============== global variables ==============

class Global
{
public: // typedefs

	// function signitures (diregard array cardinarlity) to skeleton
	typedef std::map<Function, skeleton> FuncToSkelMap;

private: // private
	FuncToSkelMap function_map; // for server only

public: //public

	// network structures
	NameService ns;
	TCP::Sockets sockets;
	Postman postman;

	// server (only set when the process is a server)
	int server_fd;
	int server_id;
	bool is_terminate;

	// binder (used by both clients and servers)
	const char *binder_hostname;
	int binder_port;

public: //methods
	Global()
		: postman(sockets, ns),
		  server_fd(-1),
		  server_id(-1),
		  is_terminate(false),
		  binder_hostname(getenv("BINDER_ADDRESS")),
		  binder_port(strtol(getenv("BINDER_PORT"), NULL, 10))
	{
		// this constructor should not throw exception
		this->sockets.set_buffer(&postman);
		assert(binder_hostname != NULL);
	}

	// set and retreive skeletons for servers (only)
	void update_func_skel(const Function &func, const skeleton &skel);
	int get_func_skel(const Function &func, skeleton &ret);

	// wait for desired request (-1 means timeout; 0 means ok)
	// desired contains flags of Postman::MessageType
	int wait_for_desired(int desired, Postman::Request &ret, int timeout = DEFAULT_TIMEOUT);
	void handle_update_log(int remote_fd);

	// this is called after server_name is reserved (either by the binder or cache)
	int rpc_call_helper(Name &server_name, Function &func, void **args);
} g;

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
	TCP::ScopedConnection conn(g.sockets, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// cannot connect to the binder
		return -1;
	}

	if(g.postman.send_iam_server(binder_fd, port) < 0)
	{
		// should not happen...
		return -1;
	}

	Postman::Request req;

	if(g.wait_for_desired(Postman::SERVER_OK, req) < 0 || g.is_terminate)
	{
		// timeout
		return -1;
	}

	std::stringstream ss(req.message.str);
	g.server_id = pop_i32(ss);
	g.ns.apply_logs(ss);
	return 0;
}

int Global::rpc_call_helper(Name &server_name, Function &func, void **args)
{
	TCP::ScopedConnection target_conn(g.sockets, server_name.ip, server_name.port);
	int target_fd = target_conn.get_fd();

	if(target_fd < 0)
	{
		// this can happen when the server (immediately) after the binder replies
		assert(false);
		return -1;
	}

	if(g.postman.send_execute(target_fd, func, args) < 0)
	{
		// couldn't send? maybe server disconnected after connection established
		return -1;
	}

	//TODO
	//assert(false);
	return -1;
}

int rpcCall(char* name, int* argTypes, void** args)
{
#ifndef NDEBUG
	std::cout << "RPC CALL" << std::endl;
#endif
	Postman::Request req;
	Function func = to_function(name, argTypes);
	// send a location request to the binder
	{
		TCP::ScopedConnection conn(g.sockets, g.binder_hostname, g.binder_port);
		int binder_fd = conn.get_fd();

		if(binder_fd < 0)
		{
			// cannot connect to the binder -- should not happen
			assert(false);
			return -1;
		}

		if(g.postman.send_loc_request(binder_fd, func) < 0)
		{
			// didn't send successfully
			return -1;
		}

		if(g.wait_for_desired(Postman::LOC_REPLY, req) < 0 || g.is_terminate)
		{
			// timeout
			return -1;
		}
	}
	std::stringstream ss(req.message.str);
	bool is_success = pop_i8(ss);
	g.ns.apply_logs(ss);

	if(is_success)
	{
		unsigned target_id = pop_i32(ss);
		Name name;

		if(g.ns.resolve(target_id, name) < 0)
		{
			// cannot happen -- the database is synced with the binder
			assert(false);
			return -1;
		}

		return g.rpc_call_helper(name, func, args);;
	}

	assert(!is_success);
	Postman::ErrorNo err = static_cast<Postman::ErrorNo>(pop_i32(ss));

	switch(err)
	{
		case Postman::NO_AVAILABLE_SERVER:
			// do something?
			return -1;

		default:
			// not supposed to happen
			assert(false);
			return -1;
	}
}

int rpcCacheCall(char* name, int* argTypes, void** args)
{
	(void)args;
	//TODO
	assert(false);
	TCP::ScopedConnection conn(g.sockets, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// cannot connect to the binder -- should not happen
		return -1;
	}

	Function func = to_function(name, argTypes);
	return -1;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
#ifndef NDEBUG
	std::cout << "RPC REGISTER" << std::endl;
#endif
	// only the server calls this methods, and hello is sent during init()
	Function func = to_function(name, argTypes);
	TCP::ScopedConnection conn(g.sockets, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// cannot connect to the binder
		assert(false);
		return -1;
	}

	assert(g.server_id != -1); // must be registered

	if(g.postman.send_register(binder_fd, g.server_id, func) < 0)
	{
		// cannot send the request
		return -1;
	}

	Postman::Request req;

	if(g.wait_for_desired(Postman::REGISTER_DONE, req) < 0 || g.is_terminate)
	{
		// cannot get a reply...
		return -1;
	}

	// register the function skeleton locally
	g.update_func_skel(func, f);
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
	while(!g.is_terminate)
	{
		Postman::Request req;

		if(g.wait_for_desired(Postman::EXECUTE, req) < 0 || g.is_terminate)
		{
			// timeout or decided to terminated
			continue;
		}

		int remote_fd = req.fd;
		unsigned remote_ns_version = req.message.ns_version;
		std::stringstream ss(req.message.str);
		Function func = func_from_sstream(ss);
		skeleton skel;

		if(remote_ns_version > g.ns.get_version())
		{
			// the client has more update-to-date logs, ask for it
			handle_update_logs(remote_fd);
		}

		if(g.get_func_skel(func, skel) < 0)
		{
			// impossible -- client knows this function exists and server has the latest logs about its functions
			assert(false);
			return -1;
		}

#ifndef NDEBUG
		std::cout << "executing " << func.name << " func_addr:" << (void*)skel << std::endl;
#endif
		//TODO
	}

	// server terminates gracefully
	return 0;
}

int rpcTerminate()
{
#ifndef NDEBUG
	std::cout << "RPC TERMINATE" << std::endl;
#endif
	TCP::ScopedConnection conn(g.sockets, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// huh? trying to tell the binder to terminate, but couln't connect to the binder..>
		return -1;
	}

	return g.postman.send_terminate(binder_fd);
}

void Global::update_func_skel(const Function &func, const skeleton &skel)
{
#ifndef NDEBUG
	std::cout << "local: registering " << func.name << " with skel address:" << (void*)skel << std::endl;
#endif
	Function sig = func.to_signiture();
	// discard previous skeleton if existed
	this->function_map[sig] = skel;
}

int Global::get_func_skel(const Function &func, skeleton &ret)
{
	Function sig = func.to_signiture();
	FuncToSkelMap::iterator it = this->function_map.find(sig);

	if(it == this->function_map.end())
	{
		// bug -- this means server doesn't know the functions it registered
		assert(false);
		return -1;
	}

	ret = it->second;
	return 0;
}

int Global::wait_for_desired(int desired, Postman::Request &ret, int timeout)
{
	Timer timer; // double-timer
	std::cout << "wait: is_terminate:" << is_terminate << std::endl;

	while(!this->is_terminate)
	{
		if(timer.is_timeout())
		{
			// timeout
			return -1;
		}

		if(postman.sync_and_receive_any(ret) < 0)
		{
			continue;
		}

		// got the desired type(s) of request
		if((ret.message.msg_type & desired) != 0)
		{
			return 0;
		}

		int remote_fd = ret.fd;
		unsigned remote_ns_version = ret.message.ns_version;
		std::stringstream ss(ret.message.str);

		switch(ret.message.msg_type)
		{
			case Postman::TERMINATE:
			{
				if(server_id == -1)
				{
					// this is a client -- not applicable (bug or "malicious attacks")
					continue;
				}

				// the server got a TERMINATE request, but the only thing we know
				// about the binder is the hostname and the listening port -- we can't
				// trust the port that is used to send this message.
				TCP::ScopedConnection conn(sockets, binder_hostname, binder_port);
				int binder_fd = conn.get_fd();

				if(binder_fd < 0)
				{
					// cannot connect to the binder
					assert(false);
					return -1;
				}

				if(postman.send_confirm_terminate(binder_fd) < 0)
				{
					// cannot ask IS_TERMINATE to the binder...
					assert(false);
					return -1;
				}

				if(wait_for_desired(Postman::CONFIRM_TERMINATE, ret) < 0)
				{
					// timeout -- assume failure
					assert(false);
					return -1;
				}

				this->is_terminate = pop_i8(ss);
				break;
			}

			case Postman::ASK_NS_UPDATE:
				this->postman.reply_ns_update(remote_fd, remote_ns_version);
				break;

			case Postman::NS_UPDATE_SENT:
				this->ns.apply_logs(ss);
				break;

			default:
				// unreachable -- cannot have other requests
				assert(false);
				break;
		}
	}

	// is terminate
	return -1; // didn't get the desired request, but is terminating
}

void Global::handle_update_log(int remote_fd)
{
	// doing best-effort updates; ignore failures
	if(postman.send_ns_update(remote_fd) < 0)
	{
		// doesn't matter -- ignore
		return;
	}

	Postman::Request req;

	if(wait_for_desired(Postman::NS_UPDATE_SENT, req) < 0)
	{
		// again doesn't matter -- ignore
		return;
	}

	std::stringstream ss(req.message.str);
	ns.apply_logs(ss);
}
