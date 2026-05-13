#include "../include/decoder.h"

#include <stdio.h>

fuzz_op_t decode_op(const uint8_t *buf) {
    fuzz_op_t op = {0};

    op.syscall_id = buf[0];
    op.arg0 = buf[1];
    op.arg1 = buf[2];
    op.arg2 = buf[3];

    return op;
}

size_t read_input(const char *path, uint8_t *buf, size_t cap) {
    FILE *fp = NULL;
    size_t len = 0;

    if (buf == NULL || cap == 0) {
        return 0;
    }

    fp = path == NULL ? stdin : fopen(path, "rb");
    if (fp == NULL) {
        return 0;
    }

    len = fread(buf, 1, cap, fp);

    if (fp != stdin) {
        fclose(fp);
    }

    return len;
}
