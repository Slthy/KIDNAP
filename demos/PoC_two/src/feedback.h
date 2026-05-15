#ifndef FEEDBACK_H
#define FEEDBACK_H

#include <stdio.h>
#include <stdlib.h>

static FILE *feedback_file = NULL;

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
    fprintf(feedback_file, "FEATURE:%s\n", name);
}

static void record_combo(const char *a, const char *b) {
    fprintf(feedback_file, "COMBO:%s+%s\n", a, b);
}

static void record_dep(const char *a, const char *b) {
    fprintf(feedback_file, "DEP:%s->%s\n", a, b);
}

#endif