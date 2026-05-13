#include "../include/feedback.h"

#include <stddef.h>
#include <string.h>

void feedback_init(feedback_t *fb) {
    if (fb != NULL) {
        memset(fb, 0, sizeof(*fb));
    }
}

void record_syscall(feedback_t *fb, int syscall_nr) {
    if (fb != NULL && syscall_nr >= 0 && syscall_nr < MAX_SYSCALLS) {
        fb->seen_syscalls[syscall_nr] = 1;
    }
}

void record_errno(feedback_t *fb, int err) {
    if (fb != NULL && err >= 0 && err < MAX_ERRNOS) {
        fb->seen_errnos[err] = 1;
    }
}

void update_sequence(feedback_t *fb, int syscall_nr) {
    if (fb != NULL) {
        fb->seq_hash = (fb->seq_hash * 1315423911u) ^ (uint64_t)(uint32_t)syscall_nr;
    }
}

#ifdef SYSCALL_FEEDBACK
static feedback_t global_feedback;

static uint32_t mix3(int first, int second, int third) {
    uint32_t hash = 2166136261u;

    hash = (hash ^ (uint32_t)first) * 16777619u;
    hash = (hash ^ (uint32_t)second) * 16777619u;
    hash = (hash ^ (uint32_t)third) * 16777619u;

    return hash;
}

void feedback_syscall(int syscall_nr) {
    record_syscall(&global_feedback, syscall_nr);
    update_sequence(&global_feedback, syscall_nr);
}

void feedback_errno(int syscall_nr, int err) {
    record_errno(&global_feedback, err);
    global_feedback.seq_hash ^= (uint64_t)mix3(syscall_nr, err, 0);
}

void feedback_seq2(int first, int second) {
    global_feedback.seq_hash ^= (uint64_t)mix3(first, second, 0) % MAX_SEQ;
}

void feedback_seq3(int first, int second, int third) {
    global_feedback.seq_hash ^= (uint64_t)mix3(first, second, third) % MAX_SEQ;
}
#endif
