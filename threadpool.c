#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "threadpool.h"

threadpool* create_threadpool(int num_threads_in_pool) {
	// Check the legacy of the parameter
	if ((num_threads_in_pool <= 0) || (num_threads_in_pool > MAXT_IN_POOL))
		return NULL;

	// Create threadpool structure and initialize it
	threadpool *tPool = (threadpool*) malloc(sizeof(threadpool));
	if (!tPool) {
		perror("malloc\n");
		return NULL;
	}

	tPool->num_threads = num_threads_in_pool;
	tPool->qsize = 0;

	tPool->threads = (pthread_t*) malloc(sizeof(pthread_t) * num_threads_in_pool);
	if (!(tPool->threads)) {
		perror("malloc\n");
		return NULL;
	}

	tPool->qhead = NULL;
	tPool->qtail = NULL;

	if (pthread_mutex_init(&tPool->qlock, NULL)) {
		perror("pthread_mutex_init\n");
		return NULL;
	}

if (pthread_cond_init(&tPool->q_not_empty, NULL)) {
		perror("pthread_cond_init\n");
		return NULL;
	}

	if (pthread_cond_init(&tPool->q_empty, NULL)) {
		perror("pthread_cond_init\n");
		return NULL;
	}

	tPool->shutdown = 0;
	tPool->dont_accept = 0;

	int i = 0;
	while (i < num_threads_in_pool) {
		if (pthread_create(&(tPool->threads[i]), NULL, do_work, (void*) tPool)) {
			perror("pthread_create\n");
			return NULL;
		}
		i++;
	}

	return tPool;
}

void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg) {
	if ((!from_me) || (!dispatch_to_here))
		return;

	work_t *pWork = (work_t*) malloc(sizeof(work_t));
	if (!pWork) {
		perror("malloc\n");
		return;
	}

	pWork->routine = dispatch_to_here;
	pWork->arg = arg;
	pWork->next = NULL;

	pthread_mutex_lock(&(from_me->qlock));

	if (from_me->dont_accept) { // destroy function has started
		fprintf(stderr, "destruction has started");
		free(pWork);
		return;
	}

	if (from_me->qsize == 0) {
		from_me->qhead = pWork;
		from_me->qtail = pWork;
		pthread_cond_signal(&(from_me->q_not_empty));
	}
	else {
		from_me->qtail->next = pWork;
		from_me->qtail = pWork;
	}

	from_me->qsize += 1;

	pthread_mutex_unlock(&(from_me->qlock));
}

void* do_work(void* p) {
	threadpool *tPool = (threadpool*) p;
	work_t *pWork; // pointer to working struct

	while (1) {
		pthread_mutex_lock(&(tPool->qlock));

		while (tPool->qsize == 0) {
			if (tPool->shutdown) {
				pthread_mutex_unlock(&(tPool->qlock));
				pthread_exit(NULL);
			}

			pthread_mutex_unlock(&(tPool->qlock));
			pthread_cond_wait(&(tPool->q_not_empty), &(tPool->qlock));

			if (tPool->shutdown) {
				pthread_mutex_unlock(&(tPool->qlock));
				pthread_exit(NULL);
			}
		}

		pWork = (work_t*) tPool->qhead;
		tPool->qsize -= 1;

		if (tPool->qsize == 0) {
			tPool->qhead = NULL;
			tPool->qtail = NULL;
		}
		else {
			tPool->qhead = pWork->next;
		}

		if ((tPool->qsize == 0) && (!tPool->shutdown)) {
			pthread_cond_signal(&(tPool->q_empty));
		}

		pthread_mutex_unlock(&(tPool->qlock));

		(pWork->routine) (pWork->arg);

free(pWork);
	}
}

	
void destroy_threadpool(threadpool* destroyme) {
	if (!destroyme)
		return;

	pthread_mutex_lock(&(destroyme->qlock));

	destroyme->dont_accept = 1;

	while (destroyme->qsize != 0) {
			int a = pthread_cond_wait(&(destroyme->q_empty), &(destroyme->qlock));
		}

	destroyme->shutdown = 1;
	pthread_cond_broadcast(&(destroyme->q_not_empty));
		pthread_mutex_unlock(&(destroyme->qlock));

	int i = 0;
	while (i < (destroyme->num_threads)) {
		pthread_join(destroyme->threads[i], NULL);
		i++;
	}

	free(destroyme->threads);
	pthread_mutex_destroy(&(destroyme->qlock));
	pthread_cond_destroy(&(destroyme->q_not_empty));
	pthread_cond_destroy(&(destroyme->q_empty));
	free(destroyme);
}
