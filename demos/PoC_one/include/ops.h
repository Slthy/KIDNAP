#ifndef OPS_H
#define OPS_H

#include <stdint.h>

typedef struct {
    uint8_t syscall_id;
    uint8_t arg0;
    uint8_t arg1;
    uint8_t arg2;
} fuzz_op_t;

typedef enum {
    OP_OPENAT = 0,
    OP_READ,
    OP_WRITE,
    OP_CLOSE,
    OP_FSTAT,
    OP_LSEEK,
    OP_PREAD64,
    OP_PWRITE64,
    OP_FCNTL,
    OP_IOCTL,
    OP_STATX,
    OP_FACCESSAT,
    OP_MKDIRAT,
    OP_UNLINKAT,
    OP_READLINKAT,
    OP_GETCWD,
    OP_MMAP,
    OP_MPROTECT,
    OP_MUNMAP,
    OP_BRK,
    OP_SOCKET,
    OP_BIND,
    OP_CONNECT,
    OP_LISTEN,
    OP_GETSOCKNAME,
    OP_GETPEERNAME,
    OP_SETSOCKOPT,
    OP_GETSOCKOPT,
    OP_SENDTO,
    OP_RECVFROM,
    OP_SHUTDOWN,
    OP_PIPE2,
    OP_DUP,
    OP_DUP3,
    OP_EVENTFD2,
    OP_GETRANDOM,
    OP_CLOCK_GETTIME,
    OP_NANOSLEEP,
    OP_GETPID,
    OP_GETPPID,
    OP_GETTID,
    OP_GETUID,
    OP_GETEUID,
    OP_GETGID,
    OP_GETEGID,
    OP_UNAME,
    OP_MAX
} syscall_op_t;

#endif
