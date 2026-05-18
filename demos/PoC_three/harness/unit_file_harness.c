#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_INPUT_SIZE 4096
#define GENERATED_UNIT "fuzz-generated.service"

static const char *TYPES[] = {"oneshot", "simple"};
static const char *EXEC_STARTS[] = {
    "/bin/true",
    "/bin/false",
    "/bin/sleep 1",
    "/bin/sh -c 'mkdir -p /tmp/systemctl-fuzz && echo generated >> /tmp/systemctl-fuzz/generated.log'",
};
static const char *RESTARTS[] = {"no", "on-failure", "always"};
static const char *BOOLS[] = {"no", "yes"};
static const char *WORKDIRS[] = {"/tmp", "/tmp/systemctl-fuzz"};
static const char *RUNTIME_DIRS[] = {"", "RuntimeDirectory=systemctl-fuzz-runtime\n"};

static int read_file(const char *path, uint8_t *buf, size_t *len_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return -1; }
    size_t len = fread(buf, 1, MAX_INPUT_SIZE, fp);
    if (ferror(fp)) { perror("fread"); fclose(fp); return -1; }
    fclose(fp);
    *len_out = len;
    return 0;
}

static int run_command(const char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        perror("execvp");
        _exit(127);
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return -1; }
    return status;
}

static void render_unit(FILE *out, const uint8_t *buf, size_t len) {
    uint8_t b0 = len > 0 ? buf[0] : 0;
    uint8_t b1 = len > 1 ? buf[1] : 0;
    uint8_t b2 = len > 2 ? buf[2] : 0;
    uint8_t b3 = len > 3 ? buf[3] : 0;
    uint8_t b4 = len > 4 ? buf[4] : 0;
    uint8_t b5 = len > 5 ? buf[5] : 0;
    uint8_t b6 = len > 6 ? buf[6] : 0;
    uint8_t b7 = len > 7 ? buf[7] : 0;

    const char *type = TYPES[b0 % (sizeof(TYPES) / sizeof(TYPES[0]))];
    const char *exec_start = EXEC_STARTS[b1 % (sizeof(EXEC_STARTS) / sizeof(EXEC_STARTS[0]))];
    const char *restart = RESTARTS[b2 % (sizeof(RESTARTS) / sizeof(RESTARTS[0]))];
    const char *remain = BOOLS[b3 % 2];
    const char *private_tmp = BOOLS[b4 % 2];
    const char *no_new_privs = BOOLS[b5 % 2];
    const char *workdir = WORKDIRS[b6 % 2];
    const char *runtime_dir = RUNTIME_DIRS[b7 % 2];

    fprintf(out,
        "[Unit]\nDescription=Generated Fuzz Service\n\n"
        "[Service]\nType=%s\nExecStart=%s\nRestart=%s\nRemainAfterExit=%s\n"
        "PrivateTmp=%s\nNoNewPrivileges=%s\nWorkingDirectory=%s\n%s",
        type, exec_start, restart, remain, private_tmp, no_new_privs, workdir, runtime_dir);
}

int main(int argc, char **argv) {
    int dry_run = getenv("UNIT_FILE_HARNESS_DRY_RUN") != NULL;
    int strict = getenv("UNIT_FILE_HARNESS_STRICT") != NULL;
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
    if (read_file(input_path, input, &len) != 0) return 1;
    if (dry_run) {
        render_unit(stdout, input, len);
        return 0;
    }

    const char *unit_dir = getenv("UNIT_FILE_DIR");
    if (!unit_dir) unit_dir = "/run/systemd/system";
    char unit_path[1024];
    if (snprintf(unit_path, sizeof(unit_path), "%s/%s", unit_dir, GENERATED_UNIT) >= (int)sizeof(unit_path)) {
        fprintf(stderr, "unit path too long\n");
        return 1;
    }
    mkdir("/tmp/systemctl-fuzz", 0755);
    FILE *unit = fopen(unit_path, "w");
    if (!unit) { perror("fopen unit"); return 1; }
    render_unit(unit, input, len);
    fclose(unit);

    const char *const reload[] = {"systemctl", "daemon-reload", NULL};
    const char *const start[] = {"systemctl", "start", GENERATED_UNIT, NULL};
    const char *const status[] = {"systemctl", "status", "--no-pager", GENERATED_UNIT, NULL};
    const char *const stop[] = {"systemctl", "stop", GENERATED_UNIT, NULL};
    const char *const reset[] = {"systemctl", "reset-failed", GENERATED_UNIT, NULL};
    const char *const *commands[] = {reload, start, status, stop, reset};
    int aggregate_status = 0;
    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
        int status_code = run_command(commands[i]);
        if (status_code != 0) aggregate_status = status_code;
    }
    return strict && aggregate_status != 0 ? 1 : 0;
}
