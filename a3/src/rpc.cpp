#include "common.hpp"
#include "debug.hpp"
#include "name_service.hpp"
#include "postman.hpp"
#include "rpc.h"
#include "sockets.hpp"
#include "tasks.hpp"
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <iostream>

// ============== global variables ==============

class Global
{
public: // typedefs

	// function signitures (diregard array cardinarlity) to skeleton
	typedef std::map<Function, skeleton> FuncToSkelMap;

private: // private
	std::set<Function> real_signitures;
	FuncToSkelMap function_map; // for server only

public: //public

	// network structures
	NameService ns;
	Postman postman;

	// server (only set when the process is a server)
	int server_fd;
	int server_id;
	bool has_run_execute;
	bool is_terminate;
	Name server_name;

	// binder (used by both clients and servers)
	const char *binder_hostname;
	int binder_port;

public: //methods
	Global()
		: postman(ns),
		  server_fd(-1),
		  server_id(-1),
		  has_run_execute(false),
		  is_terminate(false),
		  binder_hostname(getenv("BINDER_ADDRESS")),
		  binder_port(strtol(getenv("BINDER_PORT"), NULL, 10))
	{
		// this constructor should not throw exception
		assert(binder_hostname != NULL);
	}

	// set and retreive skeletons for servers (only)
	void update_func_skel(const Function &func, const skeleton &skel);
	int get_func_skel(const Function &func, std::pair<Function,skeleton> &ret);
	size_t num_func_registered() const;

	// desired contains flags of Postman::MessageType
	int wait_for_desired(int desired, Postman::Request &ret, int *need_alive_fd = NULL);

	// this is called after server_name is reserved (either by the binder or cache)
	int rpc_call_helper(Name &server_name, Function &func, void **args, bool is_force_server_run = true);

	bool check_func_name(char *name) const;
} g;

// ============== codes below ==============

int rpcInit()
{
#ifndef NDEBUG
	std::cout << "RPC INIT" << std::endl;
#endif

	if(g.server_id != -1)
	{
		return HAS_ALREADY_INIT_SERVER;
	}

	if(g.has_run_execute)
	{
		return HAS_RUN_EXECUTE;
	}

	g.server_fd = g.postman.bind_and_listen();

	if(g.server_fd < 0)
	{
		// should not happen in the student environment
		return g.server_fd;
	}

	int server_port;
	std::string server_hostname;
	get_hostname(g.server_fd, server_hostname, server_port);
#ifndef NDEBUG
	print_host_info(g.server_fd,"SERVER");
#endif
	ScopedConnection conn(g.postman, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// cannot connect to the binder
		return BINDER_UNAVAILABLE;
	}

	int retval = g.postman.send_iam_server(binder_fd, server_port);

	if(retval < 0)
	{
		return retval;
	}

	Postman::Request req;
	retval = g.wait_for_desired(Postman::SERVER_OK, req, &binder_fd);

	if(retval < 0)
	{
		return retval;
	}

	std::stringstream ss(req.message.str);
	g.server_id = pop_i32(ss);
	g.ns.apply_logs(ss);
	// resolve "my" own name for future uses
	g.ns.resolve(g.server_id, g.server_name);
	return OK;
}

int Global::rpc_call_helper(Name &server_name, Function &func, void **args, bool is_force_server_run)
{
	ScopedConnection target_conn(g.postman, server_name.ip, server_name.port);
	int target_fd = target_conn.get_fd();
	int retval;

	if(target_fd < 0)
	{
		// this can happen when the server (immediately) after the binder replies
		return CANNOT_CONNECT_TO_SERVER;
	}

	retval = g.postman.send_execute(target_fd, func, args, is_force_server_run);

	if(retval < 0)
	{
		// couldn't send? maybe server disconnected after connection established
		// or maybe is_force_server_run = true and server ran out of threads
		return retval;
	}

	Postman::Request req;
	retval = g.wait_for_desired(Postman::EXECUTE_REPLY, req, &target_fd);

	if(retval < 0)
	{
		return retval;
	}

	std::stringstream ss(req.message.str);
	g.ns.apply_logs(ss);
	retval = pop_i32(ss);

	// only has output when the RPC call is successful
	if(retval >= 0)
	{
		for(size_t i = 0; i < func.types.size(); i++)
		{
			int arg_type = func.types[i];
			size_t cardinality = get_arg_car(arg_type);

			if(is_arg_output(arg_type))
			{
				switch(get_arg_data_type(arg_type))
				{
					case ARG_CHAR:
						for(size_t j = 0; j < cardinality; j++)
						{
							((char*)args[i])[j] = pop_i8(ss);
						}

						break;

					case ARG_SHORT:
						for(size_t j = 0; j < cardinality; j++)
						{
							short *argi = reinterpret_cast<short*>(args[i]);
							argi[j] = pop_i16(ss);
						}

						break;

					case ARG_INT:
						for(size_t j = 0; j < cardinality; j++)
						{
							int *argi = reinterpret_cast<int*>(args[i]);
							argi[j] = pop_i32(ss);
						}

						break;

					case ARG_LONG:
						for(size_t j = 0; j < cardinality; j++)
						{
							long *argi = reinterpret_cast<long*>(args[i]);
							argi[j] = pop_i64(ss);
						}

						break;

					case ARG_DOUBLE:
						for(size_t j = 0; j < cardinality; j++)
						{
							double *argi = reinterpret_cast<double*>(args[i]);
							argi[j] = pop_f64(ss);
						}

						break;

					case ARG_FLOAT:
						for(size_t j = 0; j < cardinality; j++)
						{
							float *argi = reinterpret_cast<float*>(args[i]);
							argi[j] = pop_f32(ss);
						}

						break;
				}
			}
		}
	}

	return retval;
}

int rpcCall(char* name, int* argTypes, void** args)
{
#ifndef NDEBUG
	std::cout << "RPC CALL" << std::endl;
#endif

	if(g.server_id != -1)
	{
		return NOT_A_CLIENT;
	}

	// sanity check
	if(!g.check_func_name(name))
	{
		return FUNCTION_NAME_IS_INVALID;
	}

	if(argTypes == NULL)
	{
		return FUNCTION_ARGTYPES_INVALID;
	}

	int retval;
	Postman::Request req;
	Function func = to_function(name, argTypes);
	// send a location request to the binder
	{
		ScopedConnection conn(g.postman, g.binder_hostname, g.binder_port);
		int binder_fd = conn.get_fd();

		if(binder_fd < 0)
		{
			// cannot connect to the binder -- should not happen
			return BINDER_UNAVAILABLE;
		}

		retval = g.postman.send_loc_request(binder_fd, func);

		if(retval < 0)
		{
			// didn't send successfully
			return retval;
		}

		retval = g.wait_for_desired(Postman::LOC_REPLY, req, &binder_fd);

		if(retval < 0)
		{
			return retval;
		}
	}
	std::stringstream ss(req.message.str);
	bool is_success = pop_i8(ss);
	g.ns.apply_logs(ss);

	if(is_success)
	{
		unsigned target_id = pop_i32(ss);
		Name name;
		retval = g.ns.resolve(target_id, name);

		if(retval < 0)
		{
			// cannot happen -- the database is synced with the binder
			return retval;
		}

		return g.rpc_call_helper(name, func, args);
	}

	retval = pop_i32(ss);
	return retval;
}

int rpcCacheCall(char* name, int* argTypes, void** args)
{
#ifndef NDEBUG
	std::cout << "RPC cache call" << std::endl;
#endif

	if(g.server_id != -1)
	{
		return NOT_A_CLIENT;
	}

	if(argTypes == NULL)
	{
		return FUNCTION_ARGTYPES_INVALID;
	}

	// sanity check
	if(!g.check_func_name(name))
	{
		return FUNCTION_NAME_IS_INVALID;
	}

	unsigned server_id;
	Function func = to_function(name, argTypes);
	std::set<unsigned> duplicates;

	while(g.ns.suggest(g.postman, func, server_id, false) >= 0)
	{
		Name server_name;

		if(duplicates.find(server_id) != duplicates.end())
		{
			// maybe servers run out of threads; so try with rpcCall which forces the request to queue up the task
			break;
		}

		duplicates.insert(server_id);

		if(g.ns.resolve(server_id, server_name) < 0)
		{
			// bad try
			continue;
		}

		int retval = g.rpc_call_helper(server_name, func, args, false);

		if(retval >= 0 || retval == SKELETON_FAILURE)
		{
			return retval;
		}

#ifndef NDEBUG

		if(retval == SERVER_HAS_NO_AVAIL_THREADS)
		{
			std::cout << "server has no available threads; moving on to the next candadate" << std::endl;
		}

#endif
	}

	// everything failed, try one last time with the binder
	return rpcCall(name, argTypes, args);
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
#ifndef NDEBUG
	std::cout << "RPC REGISTER" << std::endl;
#endif

	if(g.server_id == -1)
	{
		return NOT_A_SERVER;
	}

	if(g.has_run_execute)
	{
		return HAS_RUN_EXECUTE;
	}

	// sanity check
	if(!g.check_func_name(name))
	{
		return FUNCTION_NAME_IS_INVALID;
	}

	if(argTypes == NULL)
	{
		return FUNCTION_ARGTYPES_INVALID;
	}

	if(f == NULL)
	{
		return SKELETON_IS_NULL;
	}

	// only the server calls this methods, and hello is sent during init()
	Function func = to_function(name, argTypes);
	int retval;
	std::pair<Function, skeleton> not_used;
	(void)not_used;

	if(g.get_func_skel(func, not_used) >= 0)
	{
		// found in local mapping -- which means the signiture is already registered; here we just need to update the skeleton locally
		g.update_func_skel(func, f);
		return SKELETON_UPDATED;
	}

	ScopedConnection conn(g.postman, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// cannot connect to the binder
		return BINDER_UNAVAILABLE;
	}

	assert(g.server_id != -1); // must be registered
	retval = g.postman.send_register(binder_fd, g.server_id, func);

	if(retval < 0)
	{
		// cannot send the request
		return retval;
	}

	Postman::Request req;
	retval = g.wait_for_desired(Postman::REGISTER_DONE, req, &binder_fd);

	if(retval < 0)
	{
		// cannot get a reply...
		return retval;
	}

	// register the function skeleton locally
	g.update_func_skel(func, f);
	// reply contains nothing but log deltas
	std::stringstream ss(req.message.str);
	g.ns.apply_logs(ss);
	return OK;
}

int rpcExecute()
{
#ifndef NDEBUG
	std::cout << "RPC EXECUTE" << std::endl;
#endif

	if(g.server_id == -1)
	{
		return NOT_A_SERVER;
	}

	if(g.has_run_execute)
	{
		return HAS_RUN_EXECUTE;
	}

	g.has_run_execute = true;

	if(g.num_func_registered() == 0)
	{
		return EXECUTE_WITHOUT_REGISTER;
	}

	int retval;
	{
		ScopedConnection conn(g.postman, g.binder_hostname, g.binder_port);
		int binder_fd = conn.get_fd();
		assert(binder_fd != -1); // something must be very wrong if the binder is down

		if(binder_fd >= 0)
		{
			// ignore error
			g.postman.send_new_server_execute(binder_fd);
		}
	}
	// used by the server to run tasks on a thread pool
	Tasks tasks;

	// only the server calls this methods, and hello is sent during init()
	while(!g.is_terminate)
	{
		Postman::Request req;

		if(g.wait_for_desired(Postman::EXECUTE, req) < 0)
		{
			continue;
		}

		int remote_fd = req.fd;
		unsigned remote_ns_version = req.message.ns_version;
		std::stringstream ss(req.message.str);
		bool is_force_queue_task = pop_i8(ss);
		Function func = func_from_sstream(ss);
		std::pair<Function, skeleton> func_info;
		retval = g.get_func_skel(func, func_info);

		if(retval < 0)
		{
			return g.postman.reply_execute(remote_fd, FUNCTION_NOT_REGISTERED, func, NULL, remote_ns_version);
		}
		else
		{
			// copy input
			std::string data(req.message.str.substr(ss.tellg()));
			Tasks::Task t(g.postman, remote_fd, g.server_name, func_info.first, func_info.second, data, remote_ns_version);

			// push call to the task queue and let other threads to handle it
			if(!tasks.push(t, is_force_queue_task))
			{
				g.postman.reply_execute(remote_fd, SERVER_HAS_NO_AVAIL_THREADS, func, NULL, remote_ns_version);
			}
		}
	}

	// kill all threads
	tasks.terminate();
	// server terminates gracefully
	return OK;
}

int rpcTerminate()
{
#ifndef NDEBUG
	std::cout << "RPC TERMINATE" << std::endl;
#endif

	if(g.server_id != -1)
	{
		return NOT_A_CLIENT;
	}

	ScopedConnection conn(g.postman, g.binder_hostname, g.binder_port);
	int binder_fd = conn.get_fd();

	if(binder_fd < 0)
	{
		// huh? trying to tell the binder to terminate, but couln't connect to the binder..>
		return BINDER_UNAVAILABLE;
	}

	return g.postman.send_terminate(binder_fd);
}

void Global::update_func_skel(const Function &func, const skeleton &skel)
{
#ifndef NDEBUG
	std::cout << "local: registering " << func.name << " with skel address:" << (void*)skel << std::endl;
#endif
	this->real_signitures.erase(func); // remove old one if existed
	this->real_signitures.insert(func); // insert the new signiture
	// discard previous skeleton if existed
	this->function_map[func] = skel;
}

int Global::get_func_skel(const Function &func, std::pair<Function,skeleton> &ret)
{
	Function sig = func.to_signiture();
	FuncToSkelMap::iterator it = this->function_map.find(sig);

	if(it == this->function_map.end())
	{
		// can occur when registering a function for the first time
		return FUNCTION_NOT_REGISTERED;
	}

	std::set<Function>::iterator fit = this->real_signitures.find(sig);
	assert(fit != this->real_signitures.end());
	ret = std::make_pair(*fit, it->second);
	return OK;
}

int Global::wait_for_desired(int desired, Postman::Request &ret, int *need_alive_fd)
{
	while(!this->is_terminate)
	{
		if(need_alive_fd != NULL && !postman.is_alive(*need_alive_fd))
		{
			// need-alive target has been disconnected, so error
			return REMOTE_DISCONNECTED;
		}

		if(postman.sync_and_receive_any(ret, need_alive_fd) < 0)
		{
			continue;
		}

		// got the desired type(s) of request
		if((ret.message.msg_type & desired) != 0)
		{
			return OK;
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
				ScopedConnection conn(postman, binder_hostname, binder_port);
				int binder_fd = conn.get_fd();

				if(binder_fd < 0)
				{
					// cannot connect to the binder
					assert(false);
					break;
				}

				if(postman.send_confirm_terminate(binder_fd) < 0)
				{
					// cannot ask IS_TERMINATE to the binder...
					assert(false);
					break;
				}

				if(wait_for_desired(Postman::CONFIRM_TERMINATE, ret, &binder_fd) < 0)
				{
					break;
				}

				this->is_terminate = pop_i8(ss);
#ifndef NDEBUG
				std::cout << "TERMINATING SERVER" << std::endl;
#endif
				break;
			}

			case Postman::NEW_SERVER_EXECUTE:
			{
				ScopedConnection conn(postman, binder_hostname, binder_port);
				int binder_fd = conn.get_fd();
				assert(binder_fd != -1); // something is wrong...

				if(binder_fd >= 0)
				{
					this->postman.send_ns_update(binder_fd);
				}

				Postman::Request req;

				if(wait_for_desired(Postman::NS_UPDATE_SENT, req, &binder_fd) >= 0)
				{
					std::stringstream ss(req.message.str);
					this->ns.apply_logs(ss);
				}

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
	return TERMINATING; // didn't get the desired request, but is terminating
}

bool Global::check_func_name(char *name) const
{
	return name != NULL && (*name != '\0' && strlen(name) <= MAX_FUNC_NAME_LEN);
}

size_t Global::num_func_registered() const
{
	return this->function_map.size();
}
