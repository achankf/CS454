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

// ============== class definitions  ==============

class FdBookKeeper;

struct BinderTrigger : public TCP::Sockets::Trigger
{
private: // references
	NameService &ns;
	FdBookKeeper &keeper;
public: // references

	BinderTrigger(NameService &ns, FdBookKeeper &keeper);
	virtual void when_connected(int fd);
	virtual void when_disconnected(int fd);
};

// since sockets doesn't clean up FdBookKeeper, a separate version
// of sync_and_receive_any is needed
//int sync_and_receive_any(Postman &postman, FdBookKeeper &keeper, Request &req);

class FdBookKeeper
{
private: // data
	unsigned next;
	std::map<int, unsigned> fd_to_id;
private: // helper
	unsigned next_u32();
public: // methods
	FdBookKeeper() : next(0) {}

	// called by BinderTrigger; returns id
	unsigned remove_fd(int fd);

	// try to find an id for the name; if not found, assign a new id
	unsigned try_resolve(int fd, NameService &ns, Name &name);
};

// ============== codes ==============

int handle_request(Postman &postman, NameService &ns, FdBookKeeper &keeper, Postman::Request &req)
{
	int remote_fd = req.fd;
	Postman::Message &msg = req.message;
	unsigned remote_ns_version = msg.ns_version;
	Name remote_name;
	unsigned remote_id = keeper.try_resolve(remote_fd, ns, remote_name);
	std::stringstream ss(msg.str);

	switch(msg.msg_type)
	{
		case Postman::I_AM_SERVER:
		{
			int listen_port = pop_i32(ss);
			ns.add_listen_port(remote_id, listen_port);
			return postman.reply_iam_server(remote_fd, remote_ns_version);
		}

		case Postman::REGISTER:
		{
			Function func = func_from_sstream(ss);
			ns.register_fn(remote_id, func);
			return postman.reply_register(remote_fd, remote_ns_version);
		}

		case Postman::LOC_REQUEST:
		{
			Function func = func_from_sstream(ss);
			return postman.reply_loc_request(remote_fd, func, remote_ns_version);
		}

		case Postman::TERMINATE:
			assert(false);
			return -1;

		default:
			// not applicable; drop request
			return 0;
	}

	// unreachable
	assert(false);
	return -1;
}

BinderTrigger::BinderTrigger(NameService &ns, FdBookKeeper &keeper)
	: ns(ns), keeper(keeper) {}

void BinderTrigger::when_connected(int fd)
{
	Name remote_name;
	unsigned remote_id = this->keeper.try_resolve(fd, ns, remote_name);
	(void) remote_id;
#ifndef NDEBUG
	std::cout << "book keeper registered fd:" << fd << " id:" << remote_id << ' ' << to_format(remote_name) << std::endl;
#endif
}

void BinderTrigger::when_disconnected(int fd)
{
	unsigned id = this->keeper.remove_fd(fd);
	this->ns.kill(id);
}

unsigned FdBookKeeper::try_resolve(int fd, NameService &ns, Name &name)
{
	if(get_peer_info(fd, name) < 0)
	{
		// should not happen
		assert(false);
		return -1;
	}

	unsigned id;
	std::map<int,unsigned>::iterator it = this->fd_to_id.find(fd);

	if(it != this->fd_to_id.end())
	{
#ifndef NDEBUG
		// if found in the book keeper, the name and its id must be found in name directory too
		int ret = ns.resolve(name, id);
		assert(ret >= 0);
		assert(id == it->second);
#endif
		return it->second;
	}

	// name does not exist, add it to the book keeper and to the directory
	id = this->next_u32();
	this->fd_to_id.insert(std::make_pair(fd,id));
	ns.register_name(id, name);
	return 0;
}

unsigned FdBookKeeper::next_u32()
{
	unsigned ret = this->next;
	this->next++;
	return ret;
}

unsigned FdBookKeeper::remove_fd(int fd)
{
	std::map<int,unsigned>::iterator it = this->fd_to_id.find(fd);
	// fails when removing a fd that did not exist... bug
	assert(it != this->fd_to_id.end());
	fd_to_id.erase(it);
	return it->second;
}

int main()
{
	NameService ns;
	FdBookKeeper keeper;
	BinderTrigger trigger(ns, keeper);
	TCP::Sockets sockets;
	Postman postman(sockets, ns);
	sockets.set_trigger(&trigger);
	sockets.set_buffer(&postman);
	int fd = sockets.bind_and_listen();
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
			int retval = handle_request(postman, ns, keeper, req);
			(void) retval;
			// otherwise there's a bug... or something hasn't been implemented
			assert(retval >= 0);
		}
	}
}
