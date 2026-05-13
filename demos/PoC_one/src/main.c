#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#include "../include/config.h"
#include "../include/ops.h"
#include "../include/executor.h"
#include "../include/feedback.h"

fuzz_op_t decode(const uint8_t *buf) {
    fuzz_op_t op;

    op.syscall_id = buf[0];
    op.arg0       = buf[1];
    op.arg1       = buf[2];
    op.arg2       = buf[3];

    return op;
}


int main(int argc, char **argv) {
    uint8_t buf[MAX_INPUT];
    size_t len = read_input(argv[1], buf, sizeof(buf));

    int prev1 = -1;
    int prev2 = -1;

    for (int i = 0; i + 4 <= len && i < MAX_OPS * 4; i += 4) {
        fuzz_op_t op = decode(buf + i);

        int sysno = select_syscall(op.syscall_id);

#ifdef SYSCALL_FEEDBACK
        feedback_syscall(sysno);
        if (prev1 != -1)
            feedback_seq2(prev1, sysno);
        if (prev2 != -1)
            feedback_seq3(prev2, prev1, sysno);
#endif

        long ret = execute_safe_syscall(sysno, op);

#ifdef SYSCALL_FEEDBACK
        if (ret < 0)
            feedback_errno(sysno, errno);
#endif

        prev2 = prev1;
        prev1 = sysno;
    }

    return 0;
}