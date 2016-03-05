#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>

// ============== id generator ==============

static unsigned next_u32()
{
	static unsigned i = 0;
	return i++;
}

void terminate_servers(Postman &postman)
{
	NameService::Names names = postman.ns.get_all_names();
	NameService::Names::const_iterator it = names.begin();

	for(; it != names.end(); it++)
	{
		ScopedConnection conn(postman, it->ip, it->port);
		int fd = conn.get_fd();

		if(postman.send_terminate(fd) < 0)
		{
			// ???? maybe the server is busy? assume it's dead
			continue;
		}

		Postman::Request req;
		Timer timer(1);

		while(!timer.is_timeout())
		{
			if(postman.sync_and_receive_any(req) < 0
			        || req.message.msg_type != Postman::CONFIRM_TERMINATE)
			{
				// timeout or drop requests (since we're terminating there's no need to process new requests)
				continue;
			}

			std::cout << "send terminate " << fd << std::endl;
			postman.send_confirm_terminate(fd);
			break;
		}
	}
}

int handle_request(Postman &postman, Postman::Request &req)
{
	NameService &ns = postman.ns;
	int remote_fd = req.fd;
	Postman::Message &msg = req.message;
	unsigned remote_ns_version = msg.ns_version;
	std::stringstream ss(msg.str);

	switch(msg.msg_type)
	{
		case Postman::I_AM_SERVER:
		{
			Name remote_name;

			// get the ip address and discard port (we use the listening port number instead)
			if(get_peer_info(remote_fd, remote_name) < 0)
			{
				// should not happen
				assert(false);
				return -1;
			}

			unsigned remote_id;
			int listen_port = pop_i32(ss);
			remote_name.port = listen_port;

			if(ns.resolve(remote_name, remote_id) >= 0)
			{
				// previous registered, and restarted running on the same machine and port number
				// remove old records
				ns.kill(remote_id);
			}

			// get a new id and then register it into the name directory
			remote_id = next_u32();
			ns.register_name(remote_id, remote_name);
			return postman.reply_server_ok(remote_fd, remote_id, remote_ns_version);
		}

		case Postman::REGISTER:
		{
			unsigned remote_id = pop_i32(ss);
			Function func = func_from_sstream(ss);
			ns.register_fn(remote_id, func);
			int retval = postman.reply_register(remote_fd, remote_ns_version);
			return retval;
		}

		case Postman::LOC_REQUEST:
		{
			Function func = func_from_sstream(ss);
			return postman.reply_loc_request(remote_fd, func, remote_ns_version);
		}

		case Postman::CONFIRM_TERMINATE:
			// the remote got a fake message (binder handles TERMINATE in terminate())
			return postman.send_confirm_terminate(remote_fd, false);

		case Postman::ASK_NS_UPDATE:
			return postman.reply_ns_update(remote_fd, remote_ns_version);

		default:
			// not applicable; drop request
			return 1;
	}

	// unreachable
	assert(false);
	return -1;
}

int main()
{
	NameService ns;
	Postman postman(ns);
	int fd = postman.bind_and_listen();
	print_host_info(fd,"BINDER");

	while(true)
	{
		Postman::Request req;

		// receive_any call sync()
		if(postman.sync_and_receive_any(req) >= 0)
		{
#ifndef NDEBUG
			debug_print_type(req);
#endif

			if(req.message.msg_type == Postman::TERMINATE)
			{
				terminate_servers(postman);
				break;
			}

			int retval = handle_request(postman, req);
			(void) retval;
			// otherwise there's a bug... or something hasn't been implemented
			assert(retval >= 0);
		}
	}
}
