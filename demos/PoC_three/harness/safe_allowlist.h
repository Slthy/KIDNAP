#ifndef SAFE_ALLOWLIST_H
#define SAFE_ALLOWLIST_H

typedef struct {
    const char *name;
    const char *argv[6];
} safe_command_t;

static const safe_command_t SAFE_COMMANDS[] = {
    {"daemon-reload", {"systemctl", "daemon-reload", NULL}},
    {"start-ok", {"systemctl", "start", "fuzz-ok.service", NULL}},
    {"stop-ok", {"systemctl", "stop", "fuzz-ok.service", NULL}},
    {"restart-ok", {"systemctl", "restart", "fuzz-ok.service", NULL}},
    {"reload-or-restart-ok", {"systemctl", "reload-or-restart", "fuzz-ok.service", NULL}},
    {"status-ok", {"systemctl", "status", "--no-pager", "fuzz-ok.service", NULL}},
    {"is-active-ok", {"systemctl", "is-active", "fuzz-ok.service", NULL}},
    {"is-enabled-ok", {"systemctl", "is-enabled", "fuzz-ok.service", NULL}},
    {"reset-failed", {"systemctl", "reset-failed", NULL}},
    {"list-services", {"systemctl", "list-units", "--type=service", "--no-pager", NULL}},
    {"show-ok", {"systemctl", "show", "fuzz-ok.service", NULL}},
    {"cat-ok", {"systemctl", "cat", "fuzz-ok.service", NULL}},
    {"enable-ok", {"systemctl", "enable", "fuzz-ok.service", NULL}},
    {"disable-ok", {"systemctl", "disable", "fuzz-ok.service", NULL}},
    {"mask-ok", {"systemctl", "mask", "fuzz-ok.service", NULL}},
    {"unmask-ok", {"systemctl", "unmask", "fuzz-ok.service", NULL}},
    {"start-fail", {"systemctl", "start", "fuzz-fail.service", NULL}},
    {"start-fs", {"systemctl", "start", "fuzz-fs.service", NULL}},
    {"start-sleep", {"systemctl", "start", "fuzz-sleep.service", NULL}},
    {"stop-sleep", {"systemctl", "stop", "fuzz-sleep.service", NULL}},
    {"try-restart-sleep", {"systemctl", "try-restart", "fuzz-sleep.service", NULL}},
    {"start-timer", {"systemctl", "start", "fuzz-timer.timer", NULL}},
    {"stop-timer", {"systemctl", "stop", "fuzz-timer.timer", NULL}},
    {"start-socket", {"systemctl", "start", "fuzz-socket.socket", NULL}},
    {"stop-socket", {"systemctl", "stop", "fuzz-socket.socket", NULL}},
    {"start-path", {"systemctl", "start", "fuzz-path.path", NULL}},
    {"stop-path", {"systemctl", "stop", "fuzz-path.path", NULL}},
};

#define SAFE_COMMAND_COUNT (sizeof(SAFE_COMMANDS) / sizeof(SAFE_COMMANDS[0]))

#endif
