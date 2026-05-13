#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>

#define MAX_SYSCALLS 512
#define MAX_ERRNOS 256
#define MAX_SEQ 1024

typedef struct {
    uint8_t syscall_id;
    uint8_t arg0;
    uint8_t arg1;
    uint8_t arg2;
} fuzz_op_t;

typedef struct {
    uint8_t seen_syscalls[MAX_SYSCALLS];
    uint8_t seen_errnos[MAX_ERRNOS];
    uint64_t seq_hash;
} feedback_t;

/* decoder */
fuzz_op_t decode(const uint8_t *buf);

/* executor */
void run_program(const uint8_t *buf, size_t len, feedback_t *fb);

/* feedback */
void feedback_init(feedback_t *fb);
void record_syscall(feedback_t *fb, int syscall_nr);
void record_errno(feedback_t *fb, int err);
void update_sequence(feedback_t *fb, int syscall_nr);

#endif