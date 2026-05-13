#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <stdint.h>

#include "ops.h"

syscall_op_t select_syscall(uint8_t syscall_id);
long execute_safe_syscall(syscall_op_t op_id, fuzz_op_t op);

#endif
