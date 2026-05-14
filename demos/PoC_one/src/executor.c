#define _GNU_SOURCE
#include "../include/executor.h"

#include "../include/config.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/stat.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#define TEMP_DIR "/tmp/afl-syscall-poc"
#define TEMP_FILE TEMP_DIR "/scratch.bin"
#define TEMP_MISSING TEMP_DIR "/missing.bin"
#define TEMP_LINK TEMP_DIR "/scratch.link"
#define TEMP_SUBDIR "scratch-dir"
#define TEMP_UNLINK "unlink-me.bin"

static uint8_t scratch[MAX_RW_SIZE];
static char path_buf[4096];
static struct stat stat_buf;
static struct statx statx_buf;
static struct utsname uname_buf;
static struct timespec timespec_buf;
static struct sockaddr_storage sockaddr_buf;
static socklen_t socklen_buf;

syscall_op_t select_syscall(uint8_t syscall_id) {
    return (syscall_op_t)(syscall_id % OP_MAX);
}

int syscall_number_for_op(syscall_op_t op_id) {
    switch (op_id) {
    case OP_OPENAT: return SYS_openat;
    case OP_READ: return SYS_read;
    case OP_WRITE: return SYS_write;
    case OP_CLOSE: return SYS_close;
    case OP_FSTAT: return SYS_fstat;
    case OP_LSEEK: return SYS_lseek;
    case OP_PREAD64: return SYS_pread64;
    case OP_PWRITE64: return SYS_pwrite64;
    case OP_FCNTL: return SYS_fcntl;
    case OP_IOCTL: return SYS_ioctl;
    case OP_STATX: return SYS_statx;
    case OP_FACCESSAT: return SYS_faccessat;
    case OP_MKDIRAT: return SYS_mkdirat;
    case OP_UNLINKAT: return SYS_unlinkat;
    case OP_READLINKAT: return SYS_readlinkat;
    case OP_GETCWD: return SYS_getcwd;
    case OP_MMAP: return SYS_mmap;
    case OP_MPROTECT: return SYS_mprotect;
    case OP_MUNMAP: return SYS_munmap;
    case OP_BRK: return SYS_brk;
    case OP_SOCKET: return SYS_socket;
    case OP_BIND: return SYS_bind;
    case OP_CONNECT: return SYS_connect;
    case OP_LISTEN: return SYS_listen;
    case OP_GETSOCKNAME: return SYS_getsockname;
    case OP_GETPEERNAME: return SYS_getpeername;
    case OP_SETSOCKOPT: return SYS_setsockopt;
    case OP_GETSOCKOPT: return SYS_getsockopt;
    case OP_SENDTO: return SYS_sendto;
    case OP_RECVFROM: return SYS_recvfrom;
    case OP_SHUTDOWN: return SYS_shutdown;
    case OP_PIPE2: return SYS_pipe2;
    case OP_DUP: return SYS_dup;
    case OP_DUP3: return SYS_dup3;
    case OP_EVENTFD2: return SYS_eventfd2;
    case OP_GETRANDOM: return SYS_getrandom;
    case OP_CLOCK_GETTIME: return SYS_clock_gettime;
    case OP_NANOSLEEP: return SYS_nanosleep;
    case OP_GETPID: return SYS_getpid;
    case OP_GETPPID: return SYS_getppid;
    case OP_GETTID: return SYS_gettid;
    case OP_GETUID: return SYS_getuid;
    case OP_GETEUID: return SYS_geteuid;
    case OP_GETGID: return SYS_getgid;
    case OP_GETEGID: return SYS_getegid;
    case OP_UNAME: return SYS_uname;
    case OP_MAX:
    default: return -1;
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

static void close_preserve_errno(int fd) {
    int saved_errno = errno;

    if (fd >= 0) {
        close(fd);
    }
    errno = saved_errno;
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

static void fill_loopback_addr(struct sockaddr_in *addr, uint8_t selector) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)(1024U + selector));
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

static int open_udp_socket(void) {
    return syscall(SYS_socket, AF_INET, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
}

static long syscall_with_temp_fd(int flags, uint8_t selector,
                                 long (*fn)(int fd, fuzz_op_t op, size_t size),
                                 fuzz_op_t op, size_t size) {
    int fd = open_temp_file(flags);
    long ret = -1;

    if (fd < 0) {
        return -1;
    }
    ret = fn(selected_fd(selector, fd), op, size);
    close_preserve_errno(fd);
    return ret;
}

static long do_read(int fd, fuzz_op_t op, size_t size) {
    (void)op;
    return syscall(SYS_read, fd, scratch, size);
}

static long do_write(int fd, fuzz_op_t op, size_t size) {
    memset(scratch, op.arg0, size);
    return syscall(SYS_write, fd, scratch, size);
}

static long do_fstat(int fd, fuzz_op_t op, size_t size) {
    (void)op;
    (void)size;
    return syscall(SYS_fstat, fd, &stat_buf);
}

static long do_lseek(int fd, fuzz_op_t op, size_t size) {
    (void)size;
    return syscall(SYS_lseek, fd, (off_t)op.arg1, op.arg2 % 4U);
}

static long do_pread64(int fd, fuzz_op_t op, size_t size) {
    return syscall(SYS_pread64, fd, scratch, size, (off_t)op.arg0);
}

static long do_pwrite64(int fd, fuzz_op_t op, size_t size) {
    memset(scratch, op.arg0, size);
    return syscall(SYS_pwrite64, fd, scratch, size, (off_t)op.arg0);
}

static long do_fcntl(int fd, fuzz_op_t op, size_t size) {
    static const int cmds[] = {F_GETFD, F_GETFL, F_SETFD, F_SETFL};

    (void)size;
    return syscall(SYS_fcntl, fd, cmds[op.arg1 % (sizeof(cmds) / sizeof(cmds[0]))], 0);
}

static long do_ioctl(int fd, fuzz_op_t op, size_t size) {
    (void)op;
    (void)size;
    return syscall(SYS_ioctl, fd, FIONREAD, &stat_buf);
}

static long mmap_region(size_t size, int prot) {
    return syscall(SYS_mmap, NULL, size, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

long execute_safe_syscall(syscall_op_t op_id, fuzz_op_t op) {
    int fd = -1;
    int fds[2] = {-1, -1};
    int sock = -1;
    long addr_ret = -1;
    long ret = -1;
    size_t size = bounded_size(op.arg1, op.arg2);
    struct sockaddr_in addr;
    int opt_value = op.arg2;
    socklen_t opt_len = sizeof(opt_value);
    struct timespec req = {0, (long)(op.arg0 % 1000U)};

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
            close_preserve_errno((int)ret);
        }
        return ret;
    case OP_READ:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_read, op, size);
    case OP_WRITE:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_write, op, size);
    case OP_CLOSE:
        return syscall(SYS_close, selected_fd(op.arg0, -1));
    case OP_FSTAT:
        return syscall_with_temp_fd(O_RDONLY | O_CREAT, op.arg0, do_fstat, op, size);
    case OP_LSEEK:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_lseek, op, size);
    case OP_PREAD64:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_pread64, op, size);
    case OP_PWRITE64:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_pwrite64, op, size);
    case OP_FCNTL:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_fcntl, op, size);
    case OP_IOCTL:
        return syscall_with_temp_fd(O_RDWR | O_CREAT, op.arg0, do_ioctl, op, size);
    case OP_STATX:
        ensure_temp_dir();
        return syscall(SYS_statx, AT_FDCWD, (op.arg0 & 1U) ? TEMP_MISSING : TEMP_FILE,
                       AT_SYMLINK_NOFOLLOW, STATX_BASIC_STATS, &statx_buf);
    case OP_FACCESSAT:
        ensure_temp_dir();
        return syscall(SYS_faccessat, AT_FDCWD,
                       (op.arg0 & 1U) ? TEMP_MISSING : TEMP_FILE, op.arg1 & 7U);
    case OP_MKDIRAT:
        ensure_temp_dir();
        ret = syscall(SYS_mkdirat, AT_FDCWD, TEMP_DIR "/" TEMP_SUBDIR, 0700);
        if (ret < 0 && errno == EEXIST) {
            errno = 0;
            return 0;
        }
        return ret;
    case OP_UNLINKAT:
        ensure_temp_dir();
        fd = syscall(SYS_openat, AT_FDCWD, TEMP_DIR "/" TEMP_UNLINK,
                     O_RDWR | O_CREAT | O_CLOEXEC, 0600);
        close_preserve_errno(fd);
        return syscall(SYS_unlinkat, AT_FDCWD, TEMP_DIR "/" TEMP_UNLINK, 0);
    case OP_READLINKAT:
        ensure_temp_dir();
        unlink(TEMP_LINK);
        if (symlink(TEMP_FILE, TEMP_LINK) != 0 && errno != EEXIST) {
            return -1;
        }
        return syscall(SYS_readlinkat, AT_FDCWD, (op.arg0 & 1U) ? TEMP_MISSING : TEMP_LINK,
                       path_buf, size);
    case OP_GETCWD:
        return syscall(SYS_getcwd, path_buf, size);
    case OP_MMAP:
        ret = mmap_region(size, PROT_READ | PROT_WRITE);
        if (ret < 0) {
            return -1;
        }
        syscall(SYS_munmap, (void *)ret, size);
        return ret;
    case OP_MPROTECT:
        addr_ret = mmap_region(size, PROT_READ | PROT_WRITE);
        if (addr_ret < 0) {
            return -1;
        }
        ret = syscall(SYS_mprotect, (void *)addr_ret, size,
                      (op.arg0 & 1U) ? PROT_NONE : PROT_READ);
        syscall(SYS_munmap, (void *)addr_ret, size);
        return ret;
    case OP_MUNMAP:
        ret = mmap_region(size, PROT_READ | PROT_WRITE);
        if (ret < 0) {
            return -1;
        }
        return syscall(SYS_munmap, (void *)ret, size);
    case OP_BRK:
        return syscall(SYS_brk, 0);
    case OP_SOCKET:
        ret = syscall(SYS_socket, selected_domain(op.arg0), selected_type(op.arg1),
                      selected_proto(op.arg2));
        if (ret >= 0) {
            close_preserve_errno((int)ret);
        }
        return ret;
    case OP_BIND:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        fill_loopback_addr(&addr, op.arg1);
        ret = syscall(SYS_bind, selected_fd(op.arg0, sock), &addr, sizeof(addr));
        close_preserve_errno(sock);
        return ret;
    case OP_CONNECT:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        fill_loopback_addr(&addr, op.arg1);
        ret = syscall(SYS_connect, selected_fd(op.arg0, sock), &addr, sizeof(addr));
        close_preserve_errno(sock);
        return ret;
    case OP_LISTEN:
        sock = syscall(SYS_socket, AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            return -1;
        }
        ret = syscall(SYS_listen, selected_fd(op.arg0, sock), op.arg1);
        close_preserve_errno(sock);
        return ret;
    case OP_GETSOCKNAME:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        memset(&sockaddr_buf, 0, sizeof(sockaddr_buf));
        socklen_buf = sizeof(sockaddr_buf);
        ret = syscall(SYS_getsockname, selected_fd(op.arg0, sock), &sockaddr_buf,
                      &socklen_buf);
        close_preserve_errno(sock);
        return ret;
    case OP_GETPEERNAME:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        memset(&sockaddr_buf, 0, sizeof(sockaddr_buf));
        socklen_buf = sizeof(sockaddr_buf);
        ret = syscall(SYS_getpeername, selected_fd(op.arg0, sock), &sockaddr_buf,
                      &socklen_buf);
        close_preserve_errno(sock);
        return ret;
    case OP_SETSOCKOPT:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        ret = syscall(SYS_setsockopt, selected_fd(op.arg0, sock), SOL_SOCKET,
                      SO_REUSEADDR, &opt_value, sizeof(opt_value));
        close_preserve_errno(sock);
        return ret;
    case OP_GETSOCKOPT:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        ret = syscall(SYS_getsockopt, selected_fd(op.arg0, sock), SOL_SOCKET,
                      SO_TYPE, &opt_value, &opt_len);
        close_preserve_errno(sock);
        return ret;
    case OP_SENDTO:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        fill_loopback_addr(&addr, op.arg0);
        memset(scratch, op.arg0, size);
        ret = syscall(SYS_sendto, selected_fd(op.arg1, sock), scratch, size, MSG_DONTWAIT,
                      &addr, sizeof(addr));
        close_preserve_errno(sock);
        return ret;
    case OP_RECVFROM:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        memset(&sockaddr_buf, 0, sizeof(sockaddr_buf));
        socklen_buf = sizeof(sockaddr_buf);
        ret = syscall(SYS_recvfrom, selected_fd(op.arg0, sock), scratch, size, MSG_DONTWAIT,
                      &sockaddr_buf, &socklen_buf);
        close_preserve_errno(sock);
        return ret;
    case OP_SHUTDOWN:
        sock = open_udp_socket();
        if (sock < 0) {
            return -1;
        }
        ret = syscall(SYS_shutdown, selected_fd(op.arg0, sock), op.arg1 % 3U);
        close_preserve_errno(sock);
        return ret;
    case OP_PIPE2:
        ret = syscall(SYS_pipe2, fds, O_CLOEXEC | ((op.arg0 & 1U) ? O_NONBLOCK : 0));
        close_preserve_errno(fds[0]);
        close_preserve_errno(fds[1]);
        return ret;
    case OP_DUP:
        fd = open_temp_file(O_RDONLY | O_CREAT);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_dup, selected_fd(op.arg0, fd));
        close_preserve_errno((int)ret);
        close_preserve_errno(fd);
        return ret;
    case OP_DUP3:
        fd = open_temp_file(O_RDONLY | O_CREAT);
        if (fd < 0) {
            return -1;
        }
        ret = syscall(SYS_dup3, selected_fd(op.arg0, fd), 100 + op.arg1, O_CLOEXEC);
        close_preserve_errno((int)ret);
        close_preserve_errno(fd);
        return ret;
    case OP_EVENTFD2:
        ret = syscall(SYS_eventfd2, (unsigned int)op.arg0,
                      EFD_CLOEXEC | ((op.arg1 & 1U) ? EFD_NONBLOCK : 0));
        if (ret >= 0) {
            close_preserve_errno((int)ret);
        }
        return ret;
    case OP_GETRANDOM:
        return syscall(SYS_getrandom, scratch, size, (op.arg0 & 1U) ? GRND_NONBLOCK : 0);
    case OP_CLOCK_GETTIME:
        return syscall(SYS_clock_gettime, op.arg0 % 12U, &timespec_buf);
    case OP_NANOSLEEP:
        return syscall(SYS_nanosleep, &req, NULL);
    case OP_GETPID:
        return syscall(SYS_getpid);
    case OP_GETPPID:
        return syscall(SYS_getppid);
    case OP_GETTID:
        return syscall(SYS_gettid);
    case OP_GETUID:
        return syscall(SYS_getuid);
    case OP_GETEUID:
        return syscall(SYS_geteuid);
    case OP_GETGID:
        return syscall(SYS_getgid);
    case OP_GETEGID:
        return syscall(SYS_getegid);
    case OP_UNAME:
        return syscall(SYS_uname, &uname_buf);
    case OP_MAX:
    default:
        errno = EINVAL;
        return -1;
    }
}
