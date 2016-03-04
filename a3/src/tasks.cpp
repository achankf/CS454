#include "common.hpp"
#include "debug.hpp"
#include "postman.hpp"
#include "tasks.hpp"
#include <cassert>
#include <cstdlib> // malloc; need to avoid warning for deleting void*
#include <exception>
#include <iostream>

void *run_thread(void *data);

Tasks::Task::Task(Postman &postman, int remote_fd, const Name &remote_name, const Function &func, const skeleton &skel, const std::string &data, int remote_ns_version)
	: postman(postman),
	  remote_fd(remote_fd),
	  remote_name(remote_name),
	  func(func),
	  skel(skel),
	  remote_ns_version(remote_ns_version) {}

void Tasks::Task::run()
{
#ifndef NDEBUG
	std::cout << "running........" << std::endl;
	print_function(this->func);
#endif
	void **args = new void*[func.types.size()];
	int *arg_types = new int[func.types.size() + 1];
	std::stringstream ss(this->data);

	// allocate space and collect input into args
	for(size_t i = 0; i < func.types.size(); i++)
	{
		int arg_type = arg_types[i] = func.types[i];
		bool is_input = is_arg_input(arg_type);
		size_t cardinality = std::max(static_cast<size_t>(1), get_arg_car(arg_type));

		switch(get_arg_data_type(arg_type))
		{
			case ARG_CHAR:
				args[i] = malloc(cardinality * sizeof(char));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{
						((char*)args[i])[j] = pop_i8(ss);
					}
				}

				break;

			case ARG_SHORT:
				args[i] = malloc(cardinality * sizeof(short));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{
						((short*)args[i])[j] = pop_i16(ss);
					}
				}

				break;

			case ARG_INT:
				args[i] = malloc(cardinality * sizeof(int));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{
						((int*)args[i])[j] = pop_i32(ss);
					}
				}

				break;

			case ARG_LONG:
				args[i] = malloc(cardinality * sizeof(long));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{ ((long*)args[i])[j] = pop_i64(ss); }
				}

				break;

			case ARG_DOUBLE:
				args[i] = malloc(cardinality * sizeof(double));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{
						((double*)args[i])[j] = pop_f64(ss);
					}
				}

				break;

			case ARG_FLOAT:
				args[i] = malloc(cardinality * sizeof(float));

				if(is_input)
				{
					for(size_t j = 0; j < cardinality; j++)
					{
						((float*)args[i])[j] = pop_f32(ss);
					}
				}

				break;
		}
	}

	arg_types[func.types.size()] = 0;
	bool success = skel(arg_types, args) >= 0;
	postman.reply_execute(remote_fd, success, func, args, remote_ns_version);

	// clean up memory before sending reply
	for(size_t i = 0; i < func.types.size(); i++)
	{
		free(args[i]);
	}

	delete []args;
}

Tasks::Tasks() : is_terminate(false)
{
	// not going to check for errors
	int retval;
	(void) retval;
	retval = sem_init(&this->task_sem, 0, 0);
	assert(retval == 0);
	retval = pthread_mutex_init(&this->task_lock, NULL);
	assert(retval == 0);

	for(int i = 0; i < MAX_THREADS; i++)
	{
		retval = pthread_create(&this->threads[i], NULL, &run_thread, static_cast<void*>(this));
		assert(retval == 0);
	}
}

Tasks::~Tasks()
{
	pthread_mutex_destroy(&this->task_lock);
	sem_destroy(&this->task_sem);
	// must call terminate() separately -- to reply to the binder that server has exited gracefully
	assert(this->is_terminate);
}

void Tasks::terminate()
{
	if(this->is_terminate)
	{
		// should only be called once -- not by the dtor, though
		assert(false);
		return;
	}

	this->is_terminate = true;

	for(int i = 0; i < MAX_THREADS; i++)
	{
		// wake up all threads so that they can check is_terminate()
		sem_post(&this->task_sem);
	}

	for(int i = 0; i < MAX_THREADS; i++)
	{
		pthread_join(this->threads[i], NULL);
	}
}

void Tasks::push(Task &t)
{
	pthread_mutex_lock(&this->task_lock);
	this->tasks.push(t);
	pthread_mutex_unlock(&this->task_lock);
	sem_post(&this->task_sem);
}

Tasks::Task Tasks::pop()
{
	assert(!this->tasks.empty());
	pthread_mutex_lock(&this->task_lock);
	Task t = this->tasks.front();
	this->tasks.pop();
	pthread_mutex_unlock(&this->task_lock);
	return t;
}

void *run_thread(void *data)
{
	Tasks &tasks = *static_cast<Tasks*>(data);

	while(true)
	{
		sem_wait(&tasks.task_sem);

		if(tasks.is_terminate)
		{
			break;
		}

		assert(!tasks.tasks.empty());
		tasks.pop().run();
	}

	return NULL;
}
