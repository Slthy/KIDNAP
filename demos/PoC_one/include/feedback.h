#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>

#define MAX_SYSCALLS 512
#define MAX_ERRNOS 256
#define MAX_SEQ 1024

typedef struct {
    uint8_t seen_syscalls[MAX_SYSCALLS];
    uint8_t seen_errnos[MAX_ERRNOS];
    uint64_t seq_hash;
} feedback_t;

void feedback_init(feedback_t *fb);
void record_syscall(feedback_t *fb, int syscall_nr);
void record_errno(feedback_t *fb, int err);
void update_sequence(feedback_t *fb, int syscall_nr);

#ifdef SYSCALL_FEEDBACK
void feedback_syscall(int syscall_nr);
void feedback_errno(int syscall_nr, int err);
void feedback_seq2(int first, int second);
void feedback_seq3(int first, int second, int third);
#endif

#endif
