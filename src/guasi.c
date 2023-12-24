/*    Copyright 2023 Davide Libenzi
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 * 
 */


#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include "guasi_dlists.h"
#include "guasi.h"



#define GUASI_WAIT_TIMEO 15
#define GUASI_INVALID_THREAD ((pthread_t) -1)
#define GUASI_WSLEEP 250000L
#define GUASI_MAXSLEEP 4000000L



struct guasi_ctx;

struct guasi_thread {
	struct dlist_head lnk;
	struct guasi_ctx *gctx;
	pthread_t thid;
};

struct guasi_request {
	struct dlist_head lnk;
	struct guasi_ctx *gctx;
	int prio;
	guasi_syscall_t proc;
	guasi_cleanup_t free;
	guasi_param_t params[GUASI_MAX_PARAMS];
	void *priv;
	void *asid;
	pthread_t thid;
	int status;
	guasi_param_t result;
	long error;
};

struct guasi_ctx {
	int min_threads, max_threads;
	int intsig;
	int num_threads;
	int max_priority;
	struct dlist_head *reqlst;
	struct dlist_head actlst;
	struct dlist_head *reslst;
	int shutdown;
	long events;
	long requests;
	pthread_mutex_t mtx;
	pthread_cond_t rcnd, ecnd;
	struct guasi_thread *thrds;
	struct dlist_head fthlst, uthlst;
};




static struct guasi_request *guasi_wait_request(struct guasi_ctx *gctx, long timeo);
static int guasi_create_thread(struct guasi_ctx *gctx, pthread_t *thid,
			       void *(*thproc)(void *), void *data);
static int guasi_add_req(struct guasi_ctx *gctx, struct guasi_request *req);
static void guasi_nullsig(int sig);
static void *guasi_thread(void *data);
static struct guasi_thread *guasi_alloc_thread(struct guasi_ctx *gctx);
static void guasi_free_thread(struct guasi_ctx *gctx, struct guasi_thread *thr);
static void guasi_release_ctx(struct guasi_ctx *gctx);
static struct guasi_request *guasi_allocreq(struct guasi_ctx *gctx);
static void guasi_freereq(struct guasi_ctx *gctx, struct guasi_request *req);




static struct guasi_request *guasi_wait_request(struct guasi_ctx *gctx, long timeo) {
	int i, error;
	struct dlist_head *pos;
	struct guasi_request *req = NULL;
	struct timeval tvnow;
	struct timespec tstimeo;

	pthread_mutex_lock(&gctx->mtx);
	if (gctx->requests == 0) {
		gettimeofday(&tvnow, NULL);
		tstimeo.tv_sec = tvnow.tv_sec + timeo;
		tstimeo.tv_nsec = tvnow.tv_usec * 1000;

		for (error = 0;
		     !gctx->shutdown && gctx->requests == 0 && error != ETIMEDOUT;) {
			pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
					     &gctx->mtx);
			error = pthread_cond_timedwait(&gctx->rcnd, &gctx->mtx, &tstimeo);
			pthread_cleanup_pop(0);
		}
	}
	if (!gctx->shutdown && gctx->requests > 0) {
		for (i = 0; i < gctx->max_priority; i++) {
			if ((pos = dlist_first(&gctx->reqlst[i])) != NULL) {
				req = DLIST_ENTRY(pos, struct guasi_request, lnk);
				dlist_del(&req->lnk);
				dlist_addt(&req->lnk, &gctx->actlst);
				req->status = GUASI_STATUS_ACTIVE;
				req->thid = pthread_self();
				gctx->requests--;
				break;
			}
		}
	}
	pthread_mutex_unlock(&gctx->mtx);

	return req;
}

static int guasi_create_thread(struct guasi_ctx *gctx, pthread_t *thid,
			       void *(*thproc)(void *), void *data) {
	int error;
	pthread_attr_t thattr;

	pthread_attr_init(&thattr);
	pthread_attr_setscope(&thattr, PTHREAD_SCOPE_SYSTEM);
	error = pthread_create(thid, &thattr, thproc, data);
	pthread_attr_destroy(&thattr);

	return error ? -1: 0;
}

static int guasi_add_req(struct guasi_ctx *gctx, struct guasi_request *req) {
	struct guasi_thread *thr = NULL;

	pthread_mutex_lock(&gctx->mtx);
	dlist_addt(&req->lnk, &gctx->reqlst[req->prio]);
	gctx->requests++;
	pthread_cond_signal(&gctx->rcnd);
	if (gctx->requests > 1 || gctx->num_threads < gctx->max_threads)
		thr = guasi_alloc_thread(gctx);
	pthread_mutex_unlock(&gctx->mtx);

	if (thr != NULL) {
		if (guasi_create_thread(gctx, &thr->thid, guasi_thread, thr) < 0) {
			pthread_mutex_lock(&gctx->mtx);
			guasi_free_thread(gctx, thr);
			pthread_mutex_unlock(&gctx->mtx);
			return -1;
		}
	}

	return 0;
}

static int guasi_process(struct guasi_ctx *gctx, struct guasi_request *req) {

	/*
	 * Time to exec the call ...
	 */
	req->result = (*req->proc)(req->priv, req->params);
	req->error = errno;

	/*
	 * Free the parameters, if needed ...
	 */
	if (req->free != NULL)
		(*req->free)(req->priv, req->params);

	/*
	 * Add to the result list ...
	 */
	pthread_mutex_lock(&gctx->mtx);
	dlist_del(&req->lnk);
	dlist_addt(&req->lnk, &gctx->reslst[req->prio]);
	req->status = GUASI_STATUS_COMPLETE;
	req->thid = GUASI_INVALID_THREAD;
	gctx->events++;
	pthread_cond_signal(&gctx->ecnd);
	pthread_mutex_unlock(&gctx->mtx);

	return 0;
}

static void guasi_nullsig(int sig) {

}

static void *guasi_thread(void *data) {
	struct guasi_thread *thr = (struct guasi_thread *) data;
	struct guasi_ctx *gctx = thr->gctx;
	int thnbr;
	struct guasi_request *req;

	/*
	 * We use "intsig" to stop syscall processing in case of cancelation.
	 * We need "intsig" to be able to interrupt the system call.
	 */
	signal(gctx->intsig, guasi_nullsig);
	siginterrupt(gctx->intsig, 1);

	pthread_mutex_lock(&gctx->mtx);
	thnbr = gctx->num_threads++;
	pthread_mutex_unlock(&gctx->mtx);
	for (; !gctx->shutdown;) {
		if ((req = guasi_wait_request(gctx, GUASI_WAIT_TIMEO)) != NULL) {
			guasi_process(gctx, req);
		} else if (thnbr >= gctx->min_threads) {
			break;
		}
	}
	pthread_mutex_lock(&gctx->mtx);
	gctx->num_threads--;
	guasi_free_thread(gctx, thr);
	pthread_mutex_unlock(&gctx->mtx);

	return NULL;
}

static struct guasi_thread *guasi_alloc_thread(struct guasi_ctx *gctx) {
	struct dlist_head *pos;
	struct guasi_thread *thr;

	if ((pos = dlist_first(&gctx->fthlst)) == NULL)
		return NULL;
	thr = DLIST_ENTRY(pos, struct guasi_thread, lnk);
	dlist_del(&thr->lnk);
	dlist_addt(&thr->lnk, &gctx->uthlst);

	return thr;
}

static void guasi_free_thread(struct guasi_ctx *gctx, struct guasi_thread *thr) {

	dlist_del(&thr->lnk);
	dlist_addh(&thr->lnk, &gctx->fthlst);
	thr->thid = GUASI_INVALID_THREAD;
}

guasi_t guasi_create(int min_threads, int max_threads, int max_priority) {
	int i;
	struct guasi_ctx *gctx;

	if (min_threads < 0 || max_threads <= 0 || max_priority < 1) {
		errno = EINVAL;
		return NULL;
	}
	if ((gctx = (struct guasi_ctx *) malloc(sizeof(struct guasi_ctx))) == NULL)
		return NULL;
	memset(gctx, 0, sizeof(struct guasi_ctx));
	gctx->num_threads = 0;
	gctx->intsig = SIGUSR1;
	gctx->min_threads = min_threads;
	gctx->max_threads = max_threads;
	gctx->max_priority = max_priority;
	gctx->shutdown = 0;
	gctx->events = 0;
	gctx->requests = 0;
	gctx->reqlst = NULL;
	gctx->thrds = NULL;
	dlist_init_head(&gctx->actlst);
	dlist_init_head(&gctx->fthlst);
	dlist_init_head(&gctx->uthlst);
	if (pthread_mutex_init(&gctx->mtx, NULL) ||
	    pthread_cond_init(&gctx->rcnd, NULL) ||
	    pthread_cond_init(&gctx->ecnd, NULL) ||
	    (gctx->reqlst = (struct dlist_head *)
	     malloc(max_priority * sizeof(struct dlist_head))) == NULL ||
	    (gctx->reslst = (struct dlist_head *)
	     malloc(max_priority * sizeof(struct dlist_head))) == NULL ||
	    (gctx->thrds = (struct guasi_thread *)
	     malloc(max_threads * sizeof(struct guasi_thread))) == NULL) {
		guasi_release_ctx(gctx);
		return NULL;
	}
	for (i = 0; i < max_priority; i++)
		dlist_init_head(&gctx->reqlst[i]);
	for (i = 0; i < max_priority; i++)
		dlist_init_head(&gctx->reslst[i]);
	for (i = 0; i < max_threads; i++) {
		dlist_addt(&gctx->thrds[i].lnk, &gctx->fthlst);
		gctx->thrds[i].thid = GUASI_INVALID_THREAD;
		gctx->thrds[i].gctx = gctx;
	}

	return (guasi_t) gctx;
}

static void guasi_release_ctx(struct guasi_ctx *gctx) {
	int i;
	long t;
	struct dlist_head *head, *pos;
	struct guasi_thread *thr;
	struct guasi_request *req;

	/*
	 * Shutting down a GUASI object requires correctly terminating all
	 * the service threads and releasing resources. We could make this
	 * call to lazily release threads and resources (through reference-counting
	 * the GUASI context, but in that way we might leave threads referencing
	 * caller data after this call returns. Better make this syncronous
	 * and handle correct service thread termination from here.
	 * So, before we signal a shutdown so that service threads waiting
	 * to get a request will cleanly exit. We also send a first round
	 * of "intsig" to active threads.
	 */
	pthread_mutex_lock(&gctx->mtx);
	gctx->shutdown++;
	pthread_cond_broadcast(&gctx->rcnd);
	if (gctx->reqlst != NULL) {
		for (i = 0; i < gctx->max_priority; i++) {
			head = &gctx->reqlst[i];
			for (pos = dlist_first(head); pos != NULL;) {
				req = DLIST_ENTRY(pos, struct guasi_request, lnk);
				pos = dlist_next(pos, head);
				dlist_del(&req->lnk);
				if (req->free != NULL)
					(*req->free)(req->priv, req->params);
				free(req);
			}
		}
	}
	gctx->requests = 0;
	for (pos = dlist_first(&gctx->actlst); pos != NULL;
	     pos = dlist_next(pos, &gctx->actlst)) {
		req = DLIST_ENTRY(pos, struct guasi_request, lnk);
		pthread_kill(req->thid, gctx->intsig);
	}
	pthread_mutex_unlock(&gctx->mtx);

	/*
	 * This is not pretty. Service threads that were waiting on the
	 * condition while trying to fetch a request, will notice the
	 * shutdown soon and will exit. Service threads that are processing
	 * a request, are sent an "intsig" to signal them the need to stop.
	 * We give them up to GUASI_MAXSLEEP useconds to give up the service
	 * thread. After that we have no other choice of calling pthread_cancel()
	 * that will very likely lead to memory/resource leakage.
	 */
	for (t = 0; gctx->num_threads > 0 && t < GUASI_MAXSLEEP; t += GUASI_WSLEEP) {
		usleep(GUASI_WSLEEP);
		pthread_mutex_lock(&gctx->mtx);
		for (pos = dlist_first(&gctx->actlst); pos != NULL;
		     pos = dlist_next(pos, &gctx->actlst)) {
			req = DLIST_ENTRY(pos, struct guasi_request, lnk);
			pthread_kill(req->thid, gctx->intsig);
		}
		pthread_mutex_unlock(&gctx->mtx);
	}

	pthread_mutex_lock(&gctx->mtx);
	for (pos = dlist_first(&gctx->uthlst); pos != NULL;
	     pos = dlist_next(pos, &gctx->uthlst)) {
		thr = DLIST_ENTRY(pos, struct guasi_thread, lnk);
		pthread_cancel(thr->thid);
	}
	for (i = 0; i < gctx->max_priority; i++) {
		head = &gctx->reslst[i];
		for (pos = dlist_first(head); pos != NULL;) {
			req = DLIST_ENTRY(pos, struct guasi_request, lnk);
			pos = dlist_next(pos, head);
			dlist_del(&req->lnk);
			free(req);
		}
	}
	pthread_mutex_unlock(&gctx->mtx);
	pthread_cond_destroy(&gctx->rcnd);
	pthread_cond_destroy(&gctx->ecnd);
	pthread_mutex_destroy(&gctx->mtx);
	free(gctx->thrds);
	free(gctx->reslst);
	free(gctx->reqlst);
	free(gctx);
}

void guasi_free(guasi_t hctx) {
	struct guasi_ctx *gctx = (struct guasi_ctx *) hctx;

	guasi_release_ctx(gctx);
}

static struct guasi_request *guasi_allocreq(struct guasi_ctx *gctx) {
	static struct guasi_request *req;

	if ((req = (struct guasi_request *) malloc(sizeof(struct guasi_request))) == NULL)
		return NULL;

	return req;
}

static void guasi_freereq(struct guasi_ctx *gctx, struct guasi_request *req) {

	free(req);
}

guasi_req_t guasi_submit(guasi_t hctx, void *priv, void *asid, int prio,
			 guasi_syscall_t proc, guasi_cleanup_t free,
			 int nparams, ...) {
	struct guasi_ctx *gctx = (struct guasi_ctx *) hctx;
	int i;
	struct guasi_request *req;
	va_list args;

	if (prio < 0 || prio >= gctx->max_priority || nparams > GUASI_MAX_PARAMS) {
		errno = EINVAL;
		return NULL;
	}
	if ((req = guasi_allocreq(gctx)) == NULL)
		return NULL;
	req->gctx = gctx;
	req->prio = prio;
	req->priv = priv;
	req->asid = asid;
	req->proc = proc;
	req->free = free;
	req->status = GUASI_STATUS_PENDING;
	req->result = (guasi_param_t) -1;
	req->error = -1;
	req->thid = GUASI_INVALID_THREAD;
	va_start(args, nparams);
	for (i = 0; i < nparams; i++)
		req->params[i] = va_arg(args, guasi_param_t);
	va_end(args);
	if (guasi_add_req(gctx, req) < 0) {
		guasi_freereq(gctx, req);
		return NULL;
	}

	return (guasi_req_t) req;
}

int guasi_fetch(guasi_t hctx, guasi_req_t *reqs, int minreqs, int maxreqs,
		long timeo) {
	struct guasi_ctx *gctx = (struct guasi_ctx *) hctx;
	int i, error, events;
	struct dlist_head *pos, *head;
	struct guasi_request *req;
	div_t dtms;
	struct timeval tvnow;
	struct timespec tstimeo;

	pthread_mutex_lock(&gctx->mtx);
	if (gctx->events < minreqs) {
		if (timeo >= 0) {
			dtms = div(timeo, 1000);
			gettimeofday(&tvnow, NULL);
			tstimeo.tv_sec = tvnow.tv_sec + dtms.quot;
			tstimeo.tv_nsec = tvnow.tv_usec * 1000L + dtms.rem * 1000000L;

			for (error = 0;
			     !gctx->shutdown && gctx->events < minreqs &&
			     error != ETIMEDOUT;) {
				pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
						     &gctx->mtx);
				error = pthread_cond_timedwait(&gctx->ecnd, &gctx->mtx,
							       &tstimeo);
				pthread_cleanup_pop(0);
			}
		} else {
			while (!gctx->shutdown && gctx->events < minreqs) {
				pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock,
						     &gctx->mtx);
				error = pthread_cond_wait(&gctx->ecnd, &gctx->mtx);
				pthread_cleanup_pop(0);
			}
		}
	}
	for (i = 0, events = 0; events < maxreqs && i < gctx->max_priority; i++) {
		head = &gctx->reslst[i];
		for (pos = dlist_first(head);
		     pos != NULL && events < maxreqs;) {
			req = DLIST_ENTRY(pos, struct guasi_request, lnk);
			pos = dlist_next(pos, head);
			dlist_del(&req->lnk);
			reqs[events++] = (guasi_req_t) req;
		}
	}
	gctx->events -= events;
	pthread_mutex_unlock(&gctx->mtx);

	return events;
}

int guasi_req_cancel(guasi_req_t hreq) {
	struct guasi_request *req = (struct guasi_request *) hreq;
	struct guasi_ctx *gctx = req->gctx;
	int status;
	guasi_cleanup_t free = NULL;

	pthread_mutex_lock(&gctx->mtx);
	if ((status = req->status) == GUASI_STATUS_PENDING) {
		dlist_del(&req->lnk);
		dlist_addt(&req->lnk, &gctx->reslst[req->prio]);
		req->status = status = GUASI_STATUS_CANCELED;
		free = req->free;
		gctx->events++;
		pthread_cond_signal(&gctx->ecnd);
	} else {
		/*
		 * If the status of the request was not PENDING, it
		 * means that the request is either executing, or
		 * on its way to be executed. If it is executing, the
		 * following signal that we send should stop it, otherwise
		 * it's too late in any case for the request to be
		 * canceled.
		 */
		if (req->thid != GUASI_INVALID_THREAD)
			pthread_kill(req->thid, gctx->intsig);
	}
	pthread_mutex_unlock(&gctx->mtx);
	/*
	 * If the status of the request was PENDING, it means that the
	 * cleanup function will never have the chance to be called inside
	 * guasi_process(), so we call it here, out of the lock. If the
	 * status was not PENDING, the cleanup function either has been
	 * already called, or it will inside guasi_process().
	 */
	if (free != NULL)
		(*free)(req->priv, req->params);

	return status;
}

int guasi_req_info(guasi_req_t hreq, struct guasi_reqinfo *rinf) {
	struct guasi_request *req = (struct guasi_request *) hreq;

	rinf->status = req->status;
	rinf->priv = req->priv;
	rinf->asid = req->asid;
	rinf->error = req->error;
	rinf->result = req->result;

	return 0;
}

void guasi_req_free(guasi_req_t hreq) {
	struct guasi_request *req = (struct guasi_request *) hreq;
	struct guasi_ctx *gctx = req->gctx;

	guasi_freereq(gctx, req);
}

