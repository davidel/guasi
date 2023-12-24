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


#if !defined(_GUASI_H)
#define _GUASI_H


#include <inttypes.h>

#ifdef __cplusplus
#define GEXTC extern "C"
#else
#define GEXTC
#endif

#define GAPI GEXTC


#define GUASI_MAX_PARAMS 16

#define GUASI_STATUS_CANCELED 0
#define GUASI_STATUS_PENDING 1
#define GUASI_STATUS_ACTIVE 2
#define GUASI_STATUS_COMPLETE 3


typedef intptr_t guasi_param_t;
typedef guasi_param_t (*guasi_syscall_t)(void *, guasi_param_t const *);
typedef void (*guasi_cleanup_t)(void *, guasi_param_t const *);

typedef struct s_guasi { } *guasi_t;
typedef struct s_guasi_req { } *guasi_req_t;

struct guasi_reqinfo {
	void *priv;
	void *asid;
	guasi_param_t result;
	long error;
	int status;
};



GAPI guasi_t guasi_create(int min_threads, int max_threads, int max_priority);
GAPI void guasi_free(guasi_t hctx);
GAPI guasi_req_t guasi_submit(guasi_t hctx, void *priv, void *asid, int prio,
			      guasi_syscall_t proc, guasi_cleanup_t free,
			      int nparams, ...);
GAPI int guasi_fetch(guasi_t hctx, guasi_req_t *reqs, int minnreqs, int maxreqs,
		     long timeo);
GAPI int guasi_req_cancel(guasi_req_t hreq);
GAPI int guasi_req_info(guasi_req_t hreq, struct guasi_reqinfo *rinf);
GAPI void guasi_req_free(guasi_req_t hreq);

#endif

