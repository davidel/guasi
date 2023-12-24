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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <signal.h>
#include <dirent.h>
#include "guasi.h"
#include "guasi_syscalls.h"



#define GTEST_MIN_THREADS 4
#define GTEST_MAX_THREADS 16
#define GTEST_MAX_PRIORITY 1

/*
 * A *really* bad example of how to use GUASI (single request at a time
 * and wait for its completion).
 */
#define GUASI_SINGLE(res, proc, hctx, timeo, ...) do { \
	guasi_req_t hreq; \
	struct guasi_reqinfo rinf; \
	if (proc(hctx, NULL, NULL, 0, __VA_ARGS__) != NULL && \
		    guasi_fetch(hctx, &hreq, 1, 1, timeo) > 0) { \
		guasi_req_info(hreq, &rinf); \
		res = (__typeof(res)) rinf.result; \
		guasi_req_free(hreq); \
	} \
} while (0)




static int exflags;
static char *buf;
static size_t bufsize = 4096;
static long gtimeo = 60 * 1000;




static unsigned long long getustime(void) {
	struct timeval tm;

	gettimeofday(&tm, NULL);

	return tm.tv_sec * 1000000ULL + tm.tv_usec;
}

static int read_direct(char const *path) {
	int fd;

	if ((fd = open(path, O_RDONLY | exflags)) == -1) {
		perror(path);
		return -1;
	}
	while (read(fd, buf, bufsize) == bufsize);
	close(fd);

	return 0;
}

static int test_direct(char const *path) {
	char *fpath;
	DIR *dir;
	struct dirent *dent;
	struct stat stbuf;

	if ((dir = opendir(path)) == NULL) {
		perror(path);
		return -1;
	}
	while ((dent = readdir(dir)) != NULL) {
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		fpath = NULL;
		if (asprintf(&fpath, "%s/%s", path, dent->d_name) < 0 ||
		    stat(fpath, &stbuf)) {
			free(fpath);
			continue;
		}
		if (S_ISDIR(stbuf.st_mode)) {
			test_direct(fpath);
		} else if (S_ISREG(stbuf.st_mode)) {
			read_direct(fpath);
		}
		free(fpath);
	}
	closedir(dir);

	return 0;
}

static int read_guasi(guasi_t hctx, char const *path) {
	int fd;
	size_t rdres;

	fd = -1;
	GUASI_SINGLE(fd, guasi__open, hctx, gtimeo, path, O_RDONLY | exflags, 0);
	if (fd == -1) {
		perror(path);
		return -1;
	}
	do {
		rdres = 0;
		GUASI_SINGLE(rdres, guasi__read, hctx, gtimeo, fd, buf, bufsize);
	} while (rdres == bufsize);
	close(fd);

	return 0;
}

static int test_guasi(guasi_t hctx, char const *path) {
	int error;
	guasi_req_t hreq;
	char *fpath;
	DIR *dir;
	struct dirent *dent;
	struct stat stbuf;

	dir = NULL;
	GUASI_SINGLE(dir, guasi__opendir, hctx, gtimeo, path);
	if (dir == NULL) {
		perror(path);
		return -1;
	}
	for (;;) {
		dent = NULL;
		GUASI_SINGLE(dent, guasi__readdir, hctx, gtimeo, dir);
		if (dent == NULL)
			break;
		if (strcmp(dent->d_name, ".") == 0 ||
		    strcmp(dent->d_name, "..") == 0)
			continue;
		if (asprintf(&fpath, "%s/%s", path, dent->d_name) < 0)
			continue;
		error = -1;
		GUASI_SINGLE(error, guasi__stat, hctx, gtimeo, fpath, &stbuf);
		if (error) {
			perror(fpath);
			free(fpath);
			continue;
		}
		if (S_ISDIR(stbuf.st_mode)) {
			test_guasi(hctx, fpath);
		} else if (S_ISREG(stbuf.st_mode)) {
			read_guasi(hctx, fpath);
		}
		free(fpath);
	}
	closedir(dir);

	return 0;
}

int main(int ac, char **av) {
	int i, min_threads = GTEST_MIN_THREADS, max_threads = GTEST_MAX_THREADS,
		max_priority = GTEST_MAX_PRIORITY;
	unsigned long long ts, te;
	guasi_t hctx;

	for (i = 1; i < ac; i++) {
		if (strcmp(av[i], "-m") == 0 ||
		    strcmp(av[i], "--min-threads") == 0) {
			if (++i < ac)
				min_threads = atoi(av[i]);
		} else if (strcmp(av[i], "-M") == 0 ||
			   strcmp(av[i], "--max-threads") == 0) {
			if (++i < ac)
				max_threads = atoi(av[i]);
		} else if (strcmp(av[i], "-P") == 0 ||
			   strcmp(av[i], "--max-priority") == 0) {
			if (++i < ac)
				max_priority = atoi(av[i]);
#if defined(O_DIRECT)
		} else if (strcmp(av[i], "-D") == 0 ||
			   strcmp(av[i], "--direct") == 0) {
			exflags |= O_DIRECT;
#endif
#if defined(O_NOATIME)
		} else if (strcmp(av[i], "-N") == 0 ||
			   strcmp(av[i], "--no-atime") == 0) {
			exflags |= O_NOATIME;
#endif
		} else if (strcmp(av[i], "-b") == 0 ||
			   strcmp(av[i], "--bufsize") == 0) {
			if (++i < ac)
				bufsize = atol(av[i]);
		} else {
			break;
		}
	}
	if ((buf = mmap(NULL, bufsize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS,
			-1, 0)) == NULL) {
		perror("mmap");
		return 1;
	}
	if ((hctx = guasi_create(min_threads, max_threads,
				 max_priority)) == NULL) {
		munmap(buf, bufsize);
		return 1;
	}
	for (; i < ac; i++) {
		fprintf(stdout, "Testing direct access on '%s' ...\n", av[i]);
		ts = getustime();
		test_direct(av[i]);
		te = getustime();
		fprintf(stdout, "Done: %llu ms\n", (te - ts) / 1000);

		fprintf(stdout, "Testing GUASI on '%s' ...\n", av[i]);
		ts = getustime();
		test_guasi(hctx, av[i]);
		te = getustime();
		fprintf(stdout, "Done: %llu ms\n", (te - ts) / 1000);
	}
	guasi_free(hctx);
	munmap(buf, bufsize);

	return 0;
}

