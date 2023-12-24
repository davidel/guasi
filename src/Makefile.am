
include_HEADERS = guasi.h guasi_syscalls.h

lib_LTLIBRARIES = libguasi.la
libguasi_la_SOURCES = guasi.c guasi_syscalls.c
libguasi_la_CFLAGS = -D_GNU_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
libguasi_la_LDFLAGS = -lpthread

