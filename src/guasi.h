/*
 *  guasi by Davide Libenzi (generic userspace async syscall implementation)
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
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

