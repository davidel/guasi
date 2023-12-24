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
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include "guasi.h"
#include "guasi_syscalls.h"




static guasi_param_t guasi_wrap__read(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) read((int) params[0], (void *) params[1],
				    (size_t) params[2]);
}

guasi_req_t guasi__read(guasi_t hctx, void *priv, void *asid, int prio,
			int fd, void *buf, size_t size) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__read, NULL, 3,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size);
}

static guasi_param_t guasi_wrap__write(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) write((int) params[0], (void const *) params[1],
				     (size_t) params[2]);
}

guasi_req_t guasi__write(guasi_t hctx, void *priv, void *asid, int prio,
			 int fd, void const *buf, size_t size) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__write, NULL, 3,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size);
}

static guasi_param_t guasi_wrap__pread(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) pread((int) params[0], (void *) params[1],
				     (size_t) params[2], (off_t) *(goff_t *) params[3]);
}

static void guasi_free__prm3(void *priv, guasi_param_t const *params) {

	free((void *) params[3]);
}

guasi_req_t guasi__pread(guasi_t hctx, void *priv, void *asid, int prio,
			 int fd, void *buf, size_t size, goff_t off) {
	goff_t *poff;

	if ((poff = (goff_t *) malloc(sizeof(goff_t))) == NULL)
		return (guasi_req_t) NULL;
	*poff = off;

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__pread, guasi_free__prm3, 4,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) poff);
}

static guasi_param_t guasi_wrap__pwrite(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) pwrite((int) params[0], (void const *) params[1],
				      (size_t) params[2], (off_t) *(goff_t *) params[3]);
}

guasi_req_t guasi__pwrite(guasi_t hctx, void *priv, void *asid, int prio,
			  int fd, void const *buf, size_t size, goff_t off) {
	goff_t *poff;

	if ((poff = (goff_t *) malloc(sizeof(goff_t))) == NULL)
		return (guasi_req_t) NULL;
	*poff = off;

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__pwrite, guasi_free__prm3, 4,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) poff);
}

static guasi_param_t guasi_wrap__sendfile(void *priv, guasi_param_t const *params) {
	guasi_param_t res;
	off_t off;
	off_t *poff = NULL;

	if (params[2] != 0) {
		off = (off_t) *(goff_t *) params[2];
		poff = &off;
	}
	res = (guasi_param_t) sendfile((int) params[0], (int) params[1],
				       poff, (size_t) params[3]);
	if (params[2] != 0)
		*(goff_t *) params[2] = (goff_t) off;

	return res;
}

guasi_req_t guasi__sendfile(guasi_t hctx, void *priv, void *asid, int prio,
			    int ofd, int ifd, goff_t *off, size_t cnt) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__sendfile, NULL, 4,
			    (guasi_param_t) ofd, (guasi_param_t) ifd, (guasi_param_t) off,
			    (guasi_param_t) cnt);
}

static guasi_param_t guasi_wrap__fsync(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) fsync((int) params[0]);
}

GAPI guasi_req_t guasi__fsync(guasi_t hctx, void *priv, void *asid, int prio,
			      int fd) {
	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__fsync, NULL, 1,
			    (guasi_param_t) fd);
}

static guasi_param_t guasi_wrap__recv(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	pfd.fd = (int) params[0];
	pfd.events = POLLIN;
	if (poll(&pfd, 1, params[4]) <= 0)
		return -1;

	return (guasi_param_t) recv((int) params[0], (void *) params[1],
				    (size_t) params[2], (int) params[3]);
}

guasi_req_t guasi__recv(guasi_t hctx, void *priv, void *asid, int prio,
			int fd, void *buf, size_t size, int flags, long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__recv, NULL, 5,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) flags, (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__recvfrom(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	pfd.fd = (int) params[0];
	pfd.events = POLLIN;
	if (poll(&pfd, 1, params[6]) <= 0)
		return -1;

	return (guasi_param_t) recvfrom((int) params[0], (void *) params[1],
					(size_t) params[2], (int) params[3],
					(struct sockaddr *) params[4], (socklen_t *) params[5]);
}

guasi_req_t guasi__recvfrom(guasi_t hctx, void *priv, void *asid, int prio,
			    int fd, void *buf, size_t size, int flags,
			    struct sockaddr *from, socklen_t *fromlen, long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__recvfrom, NULL, 7,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) flags, (guasi_param_t) from, (guasi_param_t) fromlen,
			    (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__send(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	pfd.fd = (int) params[0];
	pfd.events = POLLOUT;
	if (poll(&pfd, 1, params[4]) <= 0)
		return -1;

	return (guasi_param_t) send((int) params[0], (void const *) params[1],
				    (size_t) params[2], (int) params[3]);
}

guasi_req_t guasi__send(guasi_t hctx, void *priv, void *asid, int prio,
			int fd, void const *buf, size_t size, int flags, long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__send, NULL, 5,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) flags, (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__sendto(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	pfd.fd = (int) params[0];
	pfd.events = POLLOUT;
	if (poll(&pfd, 1, params[6]) <= 0)
		return -1;

	return (guasi_param_t) sendto((int) params[0], (void *) params[1],
				      (size_t) params[2], (int) params[3],
				      (struct sockaddr const *) params[4], (socklen_t) params[5]);
}

guasi_req_t guasi__sendto(guasi_t hctx, void *priv, void *asid, int prio,
			  int fd, void const *buf, size_t size, int flags,
			  struct sockaddr const *to, socklen_t tolen, long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__sendto, NULL, 7,
			    (guasi_param_t) fd, (guasi_param_t) buf, (guasi_param_t) size,
			    (guasi_param_t) flags, (guasi_param_t) to, (guasi_param_t) tolen,
			    (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__accept(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	pfd.fd = (int) params[0];
	pfd.events = POLLIN;
	if (poll(&pfd, 1, params[3]) <= 0)
		return -1;

	return (guasi_param_t) accept((int) params[0], (struct sockaddr *) params[1],
				      (socklen_t *) params[2]);
}

guasi_req_t guasi__accept(guasi_t hctx, void *priv, void *asid, int prio,
			  int fd, struct sockaddr *addr, socklen_t *addrlen, long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__accept, NULL, 4,
			    (guasi_param_t) fd, (guasi_param_t) addr, (guasi_param_t) addrlen,
			    (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__connect(void *priv, guasi_param_t const *params) {
	struct pollfd pfd;

	if (connect((int) params[0], (struct sockaddr *) params[1],
		    (socklen_t) params[2]) != 0) {
		if (errno != EINPROGRESS && errno != EWOULDBLOCK)
			return -1;
		pfd.fd = (int) params[0];
		pfd.events = POLLOUT;
		if (poll(&pfd, 1, params[3]) <= 0)
			return -1;
	}

	return 0;
}

guasi_req_t guasi__connect(guasi_t hctx, void *priv, void *asid, int prio,
			   int fd, struct sockaddr const *addr, socklen_t addrlen,
			   long timeo) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__connect, NULL, 4,
			    (guasi_param_t) fd, (guasi_param_t) addr, (guasi_param_t) addrlen,
			    (guasi_param_t) timeo);
}

static guasi_param_t guasi_wrap__open(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) open((char *) params[0], params[1], params[2]);
}

guasi_req_t guasi__open(guasi_t hctx, void *priv, void *asid, int prio,
			char const *name, long flags, long mode) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__open, NULL, 3,
			    (guasi_param_t) name, (guasi_param_t) flags, (guasi_param_t) mode);
}

static guasi_param_t guasi_wrap__opendir(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) opendir((char *) params[0]);
}

guasi_req_t guasi__opendir(guasi_t hctx, void *priv, void *asid, int prio,
			   char const *name) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__opendir, NULL, 1,
			    (guasi_param_t) name);
}

static guasi_param_t guasi_wrap__readdir(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) readdir((DIR *) params[0]);
}

guasi_req_t guasi__readdir(guasi_t hctx, void *priv, void *asid, int prio,
			   DIR *dir) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__readdir, NULL, 1,
			    (guasi_param_t) dir);
}

static guasi_param_t guasi_wrap__stat(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) stat((char const *) params[0], (struct stat *) params[1]);
}

guasi_req_t guasi__stat(guasi_t hctx, void *priv, void *asid, int prio,
			char const *name, struct stat *sbuf) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__stat, NULL, 2,
			    (guasi_param_t) name, (guasi_param_t) sbuf);
}

static guasi_param_t guasi_wrap__fstat(void *priv, guasi_param_t const *params) {

	return (guasi_param_t) fstat((int) params[0], (struct stat *) params[1]);
}

guasi_req_t guasi__fstat(guasi_t hctx, void *priv, void *asid, int prio,
			 int fd, struct stat *sbuf) {

	return guasi_submit(hctx, priv, asid, prio, guasi_wrap__fstat, NULL, 2,
			    (guasi_param_t) fd, (guasi_param_t) sbuf);
}

