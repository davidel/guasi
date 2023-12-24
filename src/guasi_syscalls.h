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


#if !defined(_GUASI_SYSCALLS_H)
#define _GUASI_SYSCALLS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include "guasi.h"


/*
 * Use the longlong directly to avoid messed up off_t selections
 * from userspace.
 */
typedef long long goff_t;


/*
 * FS I/O
 */
GAPI guasi_req_t guasi__read(guasi_t hctx, void *priv, void *asid, int prio,
			     int fd, void *buf, size_t size);
GAPI guasi_req_t guasi__write(guasi_t hctx, void *priv, void *asid, int prio,
			      int fd, void const *buf, size_t size);
GAPI guasi_req_t guasi__pread(guasi_t hctx, void *priv, void *asid, int prio,
			      int fd, void *buf, size_t size, goff_t off);
GAPI guasi_req_t guasi__pwrite(guasi_t hctx, void *priv, void *asid, int prio,
			       int fd, void const *buf, size_t size, goff_t off);
GAPI guasi_req_t guasi__sendfile(guasi_t hctx, void *priv, void *asid, int prio,
				 int ofd, int ifd, goff_t *off, size_t cnt);
GAPI guasi_req_t guasi__fsync(guasi_t hctx, void *priv, void *asid, int prio,
			      int fd);

/*
 * FS
 */
GAPI guasi_req_t guasi__open(guasi_t hctx, void *priv, void *asid, int prio,
			     char const *name, long flags, long mode);
GAPI guasi_req_t guasi__opendir(guasi_t hctx, void *priv, void *asid, int prio,
				char const *name);
GAPI guasi_req_t guasi__readdir(guasi_t hctx, void *priv, void *asid, int prio,
				DIR *dir);
GAPI guasi_req_t guasi__stat(guasi_t hctx, void *priv, void *asid, int prio,
			     char const *name, struct stat *sbuf);
GAPI guasi_req_t guasi__fstat(guasi_t hctx, void *priv, void *asid, int prio,
			      int fd, struct stat *sbuf);

/*
 * Network
 */
GAPI guasi_req_t guasi__recv(guasi_t hctx, void *priv, void *asid, int prio,
			     int fd, void *buf, size_t size, int flags, long timeo);
GAPI guasi_req_t guasi__recvfrom(guasi_t hctx, void *priv, void *asid, int prio,
				 int fd, void *buf, size_t size, int flags,
				 struct sockaddr *from, socklen_t *fromlen, long timeo);
GAPI guasi_req_t guasi__send(guasi_t hctx, void *priv, void *asid, int prio,
			     int fd, void const *buf, size_t size, int flags, long timeo);
GAPI guasi_req_t guasi__sendto(guasi_t hctx, void *priv, void *asid, int prio,
			       int fd, void const *buf, size_t size, int flags,
			       struct sockaddr const *to, socklen_t tolen, long timeo);
GAPI guasi_req_t guasi__accept(guasi_t hctx, void *priv, void *asid, int prio,
			       int fd, struct sockaddr *addr, socklen_t *addrlen,
			       long timeo);
GAPI guasi_req_t guasi__connect(guasi_t hctx, void *priv, void *asid, int prio,
				int fd, struct sockaddr const *addr, socklen_t addrlen,
				long timeo);

#endif

