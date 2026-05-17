#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CONFIGLET_AFL_MAP_SIZE 65536U

static FILE *feedback_file = NULL;

#ifdef CONFIGLET_FEEDBACK
#ifdef __AFL_COMPILER
extern void __afl_coverage_interesting(uint8_t val, uint32_t id);
#else
static volatile uint8_t configlet_feedback_map[CONFIGLET_AFL_MAP_SIZE];
#endif

static uint32_t feedback_mix_bytes(uint32_t hash, const char *value) {
    while (*value != '\0') {
        hash ^= (uint8_t)*value;
        hash *= 16777619u;
        value++;
    }

    return hash;
}

static uint32_t feedback_hash2(const char *kind, const char *value) {
    uint32_t hash = 2166136261u;

    hash = feedback_mix_bytes(hash, kind);
    hash = feedback_mix_bytes(hash, ":");
    return feedback_mix_bytes(hash, value);
}

static void feedback_emit_to_afl(const char *kind, const char *value) {
    uint32_t hash = feedback_hash2(kind, value);

#ifdef __AFL_COMPILER
    uint8_t interesting_value = (uint8_t)(1u << (hash & 7u));
    uint32_t interesting_id = (hash % (CONFIGLET_AFL_MAP_SIZE - 1U)) + 1U;

    __afl_coverage_interesting(interesting_value, interesting_id);
#else
    configlet_feedback_map[hash % CONFIGLET_AFL_MAP_SIZE]++;
#endif
}
#else
static void feedback_emit_to_afl(const char *kind, const char *value) {
    (void)kind;
    (void)value;
}
#endif

static void feedback_init(void) {
    const char *path = getenv("CONFIGLET_FEEDBACK_FILE");

    if (path) {
        feedback_file = fopen(path, "a");
    }

    if (!feedback_file) {
        feedback_file = stderr;
    }
}

static void feedback_close(void) {
    if (feedback_file && feedback_file != stderr) {
        fclose(feedback_file);
    }
}

static void record_feature(const char *name) {
    feedback_emit_to_afl("FEATURE", name);
    fprintf(feedback_file, "FEATURE:%s\n", name);
}

static void record_combo(const char *a, const char *b) {
    char value[128];

    snprintf(value, sizeof(value), "%s+%s", a, b);
    feedback_emit_to_afl("COMBO", value);
    fprintf(feedback_file, "COMBO:%s\n", value);
}

static void record_dep(const char *a, const char *b) {
    char value[128];

    snprintf(value, sizeof(value), "%s->%s", a, b);
    feedback_emit_to_afl("DEP", value);
    fprintf(feedback_file, "DEP:%s\n", value);
}

#endif
