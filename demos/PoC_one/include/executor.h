#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "ops.h"

long execute_safe_syscall(syscall_op_t sysno, fuzz_op_t op);

#endif