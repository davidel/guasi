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

