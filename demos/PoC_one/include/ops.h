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
    OP_MMAP,
    OP_MPROTECT,
    OP_SOCKET,
    OP_BIND,
    OP_SETSOCKOPT,
    OP_GETPID,
    OP_UNAME,
    OP_MAX
} syscall_op_t;

#endif
