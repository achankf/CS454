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

static unsigned next_u32() // responsible for generating unique ids; note: binder is NOT multi-threaded
{
	static unsigned i = 0;
	return i++;
}

void terminate_servers(Postman &postman)
{
	NameService::Names names = postman.ns.get_all_names();
	NameService::Names::iterator it;

	for(it = names.begin(); it != names.end(); it++)
	{
#ifndef NDEBUG
		std::cout << "trying to terminate " << to_format(*it) << std::endl;
#endif
		ScopedConnection conn(postman, it->ip, it->port);
		int fd = conn.get_fd();

		if(fd < 0)
		{
			// remote already dead
#ifndef NDEBUG
			std::cout << "remote already dead" << std::endl;
#endif
			continue;
		}

		if(postman.send_terminate(fd) < 0)
		{
#ifndef NDEBUG
			std::cout << "cannot send terminate for " << to_format(*it) << std::endl;
#endif
			// ???? maybe the server is busy? assume it's dead
			continue;
		}

		Postman::Request req;

		// timeout or the remote node has disconnected
		while(postman.sync_and_receive_any(req, &fd) < 0
		        && (req.fd == fd && req.message.msg_type != Postman::CONFIRM_TERMINATE));

#ifndef NDEBUG
		std::cout << "send terminate " << fd << std::endl;
#endif
		postman.send_confirm_terminate(fd);
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

		case Postman::NEW_SERVER_EXECUTE:
		{
			NameService::Names all_names = ns.get_all_names();
			NameService::Names::iterator it;

			for(it = all_names.begin(); it != all_names.end(); it++)
			{
				Name &n = *it;
				ScopedConnection conn(postman, n.ip, n.port);
				int remote_fd = conn.get_fd();

				if(remote_fd < 0)
				{
					// remote server is dead
					unsigned remote_id;

					if(ns.resolve(n, remote_id) >= 0)
					{
						// remove it ... though not really needed to do so
						ns.kill(remote_id);
					}

					continue;
				} // otherwise connection has been established

				postman.send_new_server_execute(remote_fd);
			}
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
