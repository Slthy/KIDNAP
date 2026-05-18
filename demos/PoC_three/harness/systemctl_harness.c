#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "input_parser.h"
#include "safe_allowlist.h"

#define MAX_INPUT_SIZE 4096
#define MAX_REPEAT 3

static int read_file(const char *path, uint8_t *buf, size_t *len_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("fopen");
        return -1;
    }

    size_t len = fread(buf, 1, MAX_INPUT_SIZE, fp);
    if (ferror(fp)) {
        perror("fread");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    *len_out = len;
    return 0;
}

static void print_command(const safe_command_t *cmd) {
    for (size_t i = 0; cmd->argv[i] != NULL; ++i) {
        printf("%s%s", i == 0 ? "" : " ", cmd->argv[i]);
    }
    putchar('\n');
}

static int run_command(const safe_command_t *cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        execvp(cmd->argv[0], (char *const *)cmd->argv);
        perror("execvp");
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    return status;
}

int main(int argc, char **argv) {
    int dry_run = getenv("SYSTEMCTL_HARNESS_DRY_RUN") != NULL;
    int strict = getenv("SYSTEMCTL_HARNESS_STRICT") != NULL;
    const char *input_path = NULL;

    if (argc == 2) {
        input_path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--dry-run") == 0) {
        dry_run = 1;
        input_path = argv[2];
    } else {
        fprintf(stderr, "usage: %s [--dry-run] INPUT\n", argv[0]);
        return 2;
    }

    uint8_t input[MAX_INPUT_SIZE];
    size_t len = 0;
    if (read_file(input_path, input, &len) != 0) {
        return 1;
    }

    decoded_op_t ops[SYSTEMCTL_MAX_OPS];
    size_t op_count = decode_ops(input, len, ops, SYSTEMCTL_MAX_OPS);
    int aggregate_status = 0;

    for (size_t i = 0; i < op_count; ++i) {
        const safe_command_t *cmd = &SAFE_COMMANDS[ops[i].operation % SAFE_COMMAND_COUNT];
        unsigned repeats = (ops[i].repeat % MAX_REPEAT) + 1;

        for (unsigned r = 0; r < repeats; ++r) {
            if (dry_run) {
                print_command(cmd);
            } else {
                int status = run_command(cmd);
                if (status != 0) {
                    aggregate_status = status;
                }
            }
        }
    }

    return strict && aggregate_status != 0 ? 1 : 0;
}
