#include <stdint.h>
#include <stdio.h>

#include "input_parser.h"
#include "safe_allowlist.h"

#define MAX_INPUT_SIZE 4096
#define MAX_REPEAT 3

typedef enum {
    FAMILY_META = 0,
    FAMILY_LIFECYCLE,
    FAMILY_QUERY,
    FAMILY_FAILURE,
    FAMILY_FS,
    FAMILY_TIMER,
    FAMILY_SOCKET,
    FAMILY_PATH,
} op_family_t;

static const op_family_t OP_FAMILIES[] = {
    FAMILY_META, FAMILY_LIFECYCLE, FAMILY_LIFECYCLE, FAMILY_LIFECYCLE,
    FAMILY_LIFECYCLE, FAMILY_QUERY, FAMILY_QUERY, FAMILY_QUERY,
    FAMILY_META, FAMILY_QUERY, FAMILY_QUERY, FAMILY_QUERY,
    FAMILY_META, FAMILY_META, FAMILY_META, FAMILY_META,
    FAMILY_FAILURE, FAMILY_FS, FAMILY_LIFECYCLE, FAMILY_LIFECYCLE,
    FAMILY_LIFECYCLE, FAMILY_TIMER, FAMILY_TIMER, FAMILY_SOCKET,
    FAMILY_SOCKET, FAMILY_PATH, FAMILY_PATH,
};

static volatile unsigned semantic_sink;

static int read_file(const char *path, uint8_t *buf, size_t *len_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    size_t len = fread(buf, 1, MAX_INPUT_SIZE, fp);
    fclose(fp);
    *len_out = len;
    return 0;
}

__attribute__((noinline)) static void observe_count(size_t count) {
    if (count == 0) semantic_sink ^= 1u;
    else if (count == 1) semantic_sink ^= 2u;
    else if (count < 4) semantic_sink ^= 4u;
    else if (count < 8) semantic_sink ^= 8u;
    else if (count < 16) semantic_sink ^= 16u;
    else semantic_sink ^= 32u;
}

__attribute__((noinline)) static void observe_op(unsigned op) {
    switch (op) {
        case 0: semantic_sink ^= 101u; break;
        case 1: semantic_sink ^= 102u; break;
        case 2: semantic_sink ^= 103u; break;
        case 3: semantic_sink ^= 104u; break;
        case 4: semantic_sink ^= 105u; break;
        case 5: semantic_sink ^= 106u; break;
        case 6: semantic_sink ^= 107u; break;
        case 7: semantic_sink ^= 108u; break;
        case 8: semantic_sink ^= 109u; break;
        case 9: semantic_sink ^= 110u; break;
        case 10: semantic_sink ^= 111u; break;
        case 11: semantic_sink ^= 112u; break;
        case 12: semantic_sink ^= 113u; break;
        case 13: semantic_sink ^= 114u; break;
        case 14: semantic_sink ^= 115u; break;
        case 15: semantic_sink ^= 116u; break;
        case 16: semantic_sink ^= 117u; break;
        case 17: semantic_sink ^= 118u; break;
        case 18: semantic_sink ^= 119u; break;
        case 19: semantic_sink ^= 120u; break;
        case 20: semantic_sink ^= 121u; break;
        case 21: semantic_sink ^= 122u; break;
        case 22: semantic_sink ^= 123u; break;
        case 23: semantic_sink ^= 124u; break;
        case 24: semantic_sink ^= 125u; break;
        case 25: semantic_sink ^= 126u; break;
        default: semantic_sink ^= 127u; break;
    }
}

__attribute__((noinline)) static void observe_repeat(unsigned repeats) {
    if (repeats == 1) semantic_sink ^= 201u;
    else if (repeats == 2) semantic_sink ^= 202u;
    else semantic_sink ^= 203u;
}

__attribute__((noinline)) static void observe_family(op_family_t family) {
    switch (family) {
        case FAMILY_META: semantic_sink ^= 301u; break;
        case FAMILY_LIFECYCLE: semantic_sink ^= 302u; break;
        case FAMILY_QUERY: semantic_sink ^= 303u; break;
        case FAMILY_FAILURE: semantic_sink ^= 304u; break;
        case FAMILY_FS: semantic_sink ^= 305u; break;
        case FAMILY_TIMER: semantic_sink ^= 306u; break;
        case FAMILY_SOCKET: semantic_sink ^= 307u; break;
        case FAMILY_PATH: semantic_sink ^= 308u; break;
    }
}

__attribute__((noinline)) static void observe_pair(unsigned previous, unsigned current) {
    if (previous == SAFE_COMMAND_COUNT) {
        semantic_sink ^= 401u;
        return;
    }
    if (previous == current) semantic_sink ^= 402u;
    if (previous < current) semantic_sink ^= 403u;
    else semantic_sink ^= 404u;
    if ((previous / 4u) == (current / 4u)) semantic_sink ^= 405u;
    if ((previous ^ current) & 1u) semantic_sink ^= 406u;
    if ((previous + current) % 3u == 0) semantic_sink ^= 407u;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s INPUT\n", argv[0]);
        return 2;
    }

    uint8_t input[MAX_INPUT_SIZE];
    size_t len = 0;
    if (read_file(argv[1], input, &len) != 0) return 1;

    decoded_op_t ops[SYSTEMCTL_MAX_OPS];
    size_t count = decode_ops(input, len, ops, SYSTEMCTL_MAX_OPS);
    observe_count(count);

    unsigned previous = SAFE_COMMAND_COUNT;
    for (size_t i = 0; i < count; ++i) {
        unsigned op = ops[i].operation % SAFE_COMMAND_COUNT;
        unsigned repeats = (ops[i].repeat % MAX_REPEAT) + 1;
        observe_op(op);
        observe_repeat(repeats);
        observe_family(OP_FAMILIES[op]);
        observe_pair(previous, op);
        previous = op;
    }
    return semantic_sink == 0xdeadbeefu ? 1 : 0;
}
