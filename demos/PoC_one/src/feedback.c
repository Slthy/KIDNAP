#include "../include/feedback.h"

#include "../include/config.h"

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
extern unsigned char *__afl_area_ptr __attribute__((weak));

static feedback_t global_feedback;
static volatile uint8_t fallback_feedback_map[AFL_MAP_SIZE];

static uint32_t mix_tag(uint32_t hash, const char *tag) {
    while (*tag != '\0') {
        hash = (hash ^ (uint8_t)*tag) * 16777619u;
        tag++;
    }

    return hash;
}

static uint32_t mix_u32(uint32_t hash, uint32_t value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        hash = (hash ^ (uint8_t)(value & 0xffU)) * 16777619u;
        value >>= 8;
    }

    return hash;
}

static uint32_t feature_hash1(const char *tag, int first) {
    uint32_t hash = mix_tag(2166136261u, tag);

    return mix_u32(hash, (uint32_t)first);
}

static uint32_t feature_hash2(const char *tag, int first, int second) {
    uint32_t hash = mix_tag(2166136261u, tag);

    hash = mix_u32(hash, (uint32_t)first);
    return mix_u32(hash, (uint32_t)second);
}

static uint32_t feature_hash3(const char *tag, int first, int second, int third) {
    uint32_t hash = mix_tag(2166136261u, tag);

    hash = mix_u32(hash, (uint32_t)first);
    hash = mix_u32(hash, (uint32_t)second);
    return mix_u32(hash, (uint32_t)third);
}

static void record_feature(uint32_t feature_hash) {
    uint32_t idx = feature_hash % AFL_MAP_SIZE;

    if (&__afl_area_ptr != NULL && __afl_area_ptr != NULL) {
        __afl_area_ptr[idx]++;
        return;
    }

    fallback_feedback_map[idx]++;
}

void feedback_syscall(int syscall_nr) {
    record_syscall(&global_feedback, syscall_nr);
    update_sequence(&global_feedback, syscall_nr);
    record_feature(feature_hash1("SYSCALL", syscall_nr));
}

void feedback_errno(int syscall_nr, int err) {
    record_errno(&global_feedback, err);
    global_feedback.seq_hash ^= (uint64_t)feature_hash2("ERRNO", syscall_nr, err);
    record_feature(feature_hash2("ERRNO", syscall_nr, err));
}

void feedback_seq2(int first, int second) {
    uint32_t hash = feature_hash2("SEQ2", first, second);

    global_feedback.seq_hash ^= (uint64_t)(hash % MAX_SEQ);
    record_feature(hash);
}

void feedback_seq3(int first, int second, int third) {
    uint32_t hash = feature_hash3("SEQ3", first, second, third);

    global_feedback.seq_hash ^= (uint64_t)(hash % MAX_SEQ);
}
#endif
