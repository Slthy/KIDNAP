#define _GNU_SOURCE
#include "../include/executor.h"

#include "../include/config.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#define TEMP_DIR "/tmp/afl-syscall-poc"
#define TEMP_FILE TEMP_DIR "/scratch.bin"
#define TEMP_MISSING TEMP_DIR "/missing.bin"

static uint8_t scratch[MAX_RW_SIZE];
static struct stat stat_buf;
static struct utsname uname_buf;

syscall_op_t select_syscall(uint8_t syscall_id) {
    return (syscall_op_t)(syscall_id % OP_MAX);
}

int syscall_number_for_op(syscall_op_t op_id) {
    switch (op_id) {
    case OP_OPENAT:
        return SYS_openat;
    case OP_READ:
        return SYS_read;
    case OP_WRITE:
        return SYS_write;
    case OP_CLOSE:
        return SYS_close;
    case OP_FSTAT:
        return SYS_fstat;
    case OP_MMAP:
        return SYS_mmap;
    case OP_MPROTECT:
        return SYS_mprotect;
    case OP_SOCKET:
        return SYS_socket;
    case OP_BIND:
        return SYS_bind;
    case OP_SETSOCKOPT:
        return SYS_setsockopt;
    case OP_GETPID:
        return SYS_getpid;
    case OP_UNAME:
        return SYS_uname;
    case OP_MAX:
    default:
        return -1;
    }
}

static size_t bounded_size(uint8_t low, uint8_t high) {
    size_t size = ((size_t)high << 8) | low;

    size = (size % MAX_RW_SIZE) + 1U;
    return size > MAX_RW_SIZE ? MAX_RW_SIZE : size;
}

static void ensure_temp_dir(void) {
    if (mkdir(TEMP_DIR, 0700) != 0 && errno != EEXIST) {
        return;
    }
}

static int open_temp_file(int flags) {
    ensure_temp_dir();
    return open(TEMP_FILE, flags | O_CLOEXEC, 0600);
}

static int selected_fd(uint8_t selector, int valid_fd) {
    switch (selector & 3U) {
    case 0:
        return -1;
    case 1:
        return valid_fd;
    case 2:
        return 1024 + (int)selector;
    default:
        return 32768 + (int)selector;
    }
}

static int selected_domain(uint8_t selector) {
    static const int domains[] = {AF_INET, AF_INET6, AF_UNIX, AF_UNSPEC};

    return domains[selector % (sizeof(domains) / sizeof(domains[0]))];
}

static int selected_type(uint8_t selector) {
    static const int types[] = {SOCK_STREAM, SOCK_DGRAM, SOCK_RAW, SOCK_SEQPACKET};

    return types[selector % (sizeof(types) / sizeof(types[0]))] | SOCK_CLOEXEC;
}

static int selected_proto(uint8_t selector) {
    static const int protos[] = {0, IPPROTO_TCP, IPPROTO_UDP, 255};

    return protos[selector % (sizeof(protos) / sizeof(protos[0]))];
}

long execute_safe_syscall(syscall_op_t op_id, fuzz_op_t op) {
    int fd = -1;
    int sock = -1;
    long ret = -1;
    size_t size = bounded_size(op.arg1, op.arg2);

    errno = 0;

    switch (op_id) {
    case OP_OPENAT:
        ensure_temp_dir();
        if ((op.arg0 & 1U) != 0) {
            return syscall(SYS_openat, AT_FDCWD, TEMP_MISSING, O_RDONLY | O_CLOEXEC, 0);
        }
        ret = syscall(SYS_openat, AT_FDCWD, TEMP_FILE, O_RDWR | O_CREAT | O_CLOEXEC,
                      0600);
        if (ret >= 0) {
            close((int)ret);
        }
        return ret;
    case OP_READ:
        fd = open_temp_file(O_RDWR | O_CREAT);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_read, selected_fd(op.arg0, fd), scratch, size);
        if (ret < 0) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return ret;
        }
        close(fd);
        return ret;
    case OP_WRITE:
        fd = open_temp_file(O_RDWR | O_CREAT);
        if (fd < 0) {
            return -1;
        }
        memset(scratch, op.arg0, size);
        ret = syscall(SYS_write, selected_fd(op.arg0, fd), scratch, size);
        if (ret < 0) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return ret;
        }
        close(fd);
        return ret;
    case OP_CLOSE:
        return syscall(SYS_close, selected_fd(op.arg0, -1));
    case OP_FSTAT:
        fd = open_temp_file(O_RDONLY | O_CREAT);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_fstat, selected_fd(op.arg0, fd), &stat_buf);
        if (ret < 0) {
            int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return ret;
        }
        close(fd);
        return ret;
    case OP_MMAP:
        ret = syscall(SYS_mmap, NULL, size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ret < 0) {
            return -1;
        }
        munmap((void *)ret, size);
        return ret;
    case OP_MPROTECT:
        return syscall(SYS_mprotect, NULL, size, PROT_NONE);
    case OP_SOCKET:
        ret = syscall(SYS_socket, selected_domain(op.arg0), selected_type(op.arg1),
                      selected_proto(op.arg2));
        if (ret >= 0) {
            close((int)ret);
        }
        return ret;
    case OP_BIND:
        return syscall(SYS_bind, selected_fd(op.arg0, -1), NULL, 0);
    case OP_SETSOCKOPT:
        sock = syscall(SYS_socket, AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            return -1;
        }
        ret = syscall(SYS_setsockopt, selected_fd(op.arg0, sock), SOL_SOCKET,
                      SO_REUSEADDR, &op.arg2, sizeof(op.arg2));
        if (ret < 0) {
            int saved_errno = errno;
            close(sock);
            errno = saved_errno;
            return ret;
        }
        close(sock);
        return ret;
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
