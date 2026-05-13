#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/config.h"
#include "../include/decoder.h"
#include "../include/executor.h"
#include "../include/feedback.h"

static int run_input(const uint8_t *buf, size_t len) {
#ifdef SYSCALL_FEEDBACK
    int prev1 = -1;
    int prev2 = -1;
#endif
    size_t op_limit = MAX_OPS * OP_SIZE;
    size_t usable = len < op_limit ? len : op_limit;

    for (size_t i = 0; i + OP_SIZE <= usable; i += OP_SIZE) {
        fuzz_op_t op = decode_op(buf + i);
        syscall_op_t op_id = select_syscall(op.syscall_id);
        long ret = 0;

#ifdef SYSCALL_FEEDBACK
        int sysno = (int)op_id;
        feedback_syscall(sysno);
        if (prev1 != -1) {
            feedback_seq2(prev1, sysno);
        }
        if (prev2 != -1) {
            feedback_seq3(prev2, prev1, sysno);
        }
#endif

        errno = 0;
        ret = execute_safe_syscall(op_id, op);

#ifdef SYSCALL_FEEDBACK
        if (ret < 0) {
            feedback_errno(sysno, errno);
        }
#else
        (void)ret;
#endif
#ifdef SYSCALL_FEEDBACK
        prev2 = prev1;
        prev1 = sysno;
#endif
    }

    return 0;
}

int main(int argc, char **argv) {
    uint8_t buf[MAX_INPUT] = {0};
    const char *input_path = NULL;
    size_t len = 0;

    if (argc > 2) {
        fprintf(stderr, "usage: %s [input-file]\n", argv[0]);
        return 2;
    }

    input_path = argc == 2 ? argv[1] : NULL;
    len = read_input(input_path, buf, sizeof(buf));
    if (len == 0) {
        return 0;
    }

    return run_input(buf, len);
}
