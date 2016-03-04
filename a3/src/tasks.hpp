#ifndef _task_hpp_
#define _task_hpp_

#include "config.hpp"
#include "name_service.hpp"
#include "rpc.h"
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <vector>

class Postman;

// all methods are synchronized by a mutex and a semaphore
class Tasks
{
public: // typedefs

	class Task
	{
	private: // data
		Postman &postman;
		const int remote_fd;
		const Name remote_name; // copy
		const Function func; // copy
		const skeleton skel; // copy
		const std::string data; // copy
		const int remote_ns_version;

	public: // methods
		Task(Postman &postman, int remote_fd, const Name &name, const Function &func, const skeleton &skel, const std::string &string, int remote_ns_version);
		void run();
	};

private: // data
	std::queue<Task> tasks;
	pthread_t threads[MAX_THREADS];
	pthread_mutex_t task_lock; // prevent conflict b/w Tasks obj and threads
	sem_t task_sem; // notify threads
	bool is_terminate;

private: // methods
	Task pop();

public: // methods
	Tasks();
	~Tasks();

	void terminate();
	void push(Task &t);

	friend void *run_thread(void *data);
};

#endif
