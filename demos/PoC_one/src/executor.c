#define _GNU_SOURCE
#include "../include/executor.h"

#include "../include/config.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

static uint8_t scratch[MAX_RW_SIZE];
static struct stat stat_buf;
static struct utsname uname_buf;

syscall_op_t select_syscall(uint8_t syscall_id) {
    return (syscall_op_t)(syscall_id % OP_MAX);
}

static size_t bounded_size(uint8_t value) {
    size_t size = (size_t)value + 1U;

    return size > MAX_RW_SIZE ? MAX_RW_SIZE : size;
}

long execute_safe_syscall(syscall_op_t op_id, fuzz_op_t op) {
    int fd = -1;
    int sock = -1;
    long ret = -1;
    size_t size = bounded_size(op.arg0);

    errno = 0;

    switch (op_id) {
    case OP_OPENAT:
        return syscall(SYS_openat, AT_FDCWD, "/dev/null", O_RDONLY | O_CLOEXEC, 0);
    case OP_READ:
        fd = open("/dev/zero", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_read, fd, scratch, size);
        close(fd);
        return ret;
    case OP_WRITE:
        fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (fd < 0) {
            return -1;
        }
        memset(scratch, op.arg1, size);
        ret = syscall(SYS_write, fd, scratch, size);
        close(fd);
        return ret;
    case OP_CLOSE:
        return syscall(SYS_close, -1);
    case OP_FSTAT:
        fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_fstat, fd, &stat_buf);
        close(fd);
        return ret;
    case OP_MMAP:
        ret = (long)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if ((void *)ret == MAP_FAILED) {
            return -1;
        }
        munmap((void *)ret, size);
        return ret;
    case OP_MPROTECT:
        return syscall(SYS_mprotect, NULL, 0, PROT_NONE);
    case OP_SOCKET:
        sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            return -1;
        }
        close(sock);
        return sock;
    case OP_BIND:
        return syscall(SYS_bind, -1, NULL, 0);
    case OP_SETSOCKOPT:
        return syscall(SYS_setsockopt, -1, SOL_SOCKET, SO_REUSEADDR, &op.arg2, sizeof(op.arg2));
    case OP_GETPID:
        return syscall(SYS_getpid);
    case OP_UNAME:
        return syscall(SYS_uname, &uname_buf);
    case OP_MAX:
    default:
        errno = EINVAL;
        return -1;
    }
}
