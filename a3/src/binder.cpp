#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>

class FdBookKeeper
{
private: // data
	unsigned int next;
	std::map<int, unsigned int> fd_to_id;
public: // methods
	FdBookKeeper() : next(1) {}

	void cleanup_disconnected(NameService &ns, const TCP::Sockets::Fds &connected)
	{
		// collect all ids in the name directory
		std::vector<int> sorted_fds;
		std::map<int, unsigned int>::iterator it = fd_to_id.begin();

		for(; it != fd_to_id.end(); it++)
		{
			sorted_fds.push_back(it->first);
		}

		std::vector<int> disconnected;
		std::set_difference(
		    sorted_fds.begin(), sorted_fds.end(),
		    connected.begin(), connected.end(),
		    std::back_inserter(disconnected));

		for(size_t i = 0; i < disconnected.size(); i++)
		{
			unsigned int id = this->fd_to_id.find(disconnected[i])->second;
#ifndef NDEBUG
			std::cout << "Binder cleaning up fd:" << disconnected[i] << " id:" << id << std::endl;
#endif
			ns.kill(id);
			this->fd_to_id.erase(disconnected[i]);
		}
	}

	unsigned int next_u32()
	{
		unsigned int ret = this->next;
		this->next++;
		return ret;
	}

	unsigned int try_resolve(int fd, NameService &ns, Name &name)
	{
		if(get_peer_info(fd, name) < 0)
		{
			// should not happen
			assert(false);
			return -1;
		}

		unsigned int id;
		std::map<int,unsigned int>::iterator it = this->fd_to_id.find(fd);

		if(it != this->fd_to_id.end())
		{
#ifndef NDEBUG
			int ret = ns.resolve(name, id);
			assert(ret >= 0 && id == it->second);
#endif
			return it->second;
		}

#ifndef NDEBUG
		unsigned int not_used;
		int ret = ns.resolve(name, not_used);
		assert(ret < 0);
#endif
		id = this->next_u32();
		this->fd_to_id.insert(std::make_pair(fd,id));
		return ns.register_name(id, name);
	}
};

int handle_request(Postman &postman, NameService &ns, FdBookKeeper &keeper, Postman::Request &req)
{
	int fd = req.fd;
	Postman::Message &msg = req.message;
	unsigned int remote_ns_version = msg.ns_version;

	switch(msg.msg_type)
	{
		case Postman::HELLO:
		{
			assert(msg.size == 1);
			Name name;
			unsigned int id = keeper.try_resolve(fd, ns, name);
#ifndef NDEBUG
			std::cout << "Hello: fd:" << fd << " id:" << id << std::endl;
#endif
			return postman.reply_hello(fd, remote_ns_version);
		}

		case Postman::UPDATE_NS:
			return -1;

		case Postman::REGISTER:
			return -1;

		default:
			// not implemented or should not happen
			assert(false);
			return -1;
	}

	// unreachable
	assert(false);
	return -1;
}

int main()
{
	NameService ns;
	TCP::Sockets sockets;
	int fd = sockets.bind_and_listen();
	print_host_info(fd,"BINDER");
	Postman postman(sockets, ns);
	FdBookKeeper keeper;

	while(true)
	{
		Postman::Request req;

		// receive_any call sync()
		if(postman.receive_any(req) >= 0)
		{
			debug_print_type(req);
			handle_request(postman, ns, keeper, req);
		}

		keeper.cleanup_disconnected(ns, postman.all_connected());
	}
}
