#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "feedback.h"

#define MAX_INPUT 8192

typedef struct {
    int auth;
    int cache;
    int tls;
    int export_enabled;
    int backup;
    int metrics;

    char db[32];
    char role[32];
    char compress[32];
    char format[32];
} Config;

static int contains_line(const char *input, const char *needle) {
    return strstr(input, needle) != NULL;
}

static void parse_config(const char *input, Config *cfg) {
    memset(cfg, 0, sizeof(Config));

    strcpy(cfg->db, "none");
    strcpy(cfg->role, "user");
    strcpy(cfg->compress, "none");
    strcpy(cfg->format, "txt");

    if (contains_line(input, "AUTH=1")) cfg->auth = 1;
    if (contains_line(input, "CACHE=1")) cfg->cache = 1;
    if (contains_line(input, "TLS=1")) cfg->tls = 1;
    if (contains_line(input, "EXPORT=1")) cfg->export_enabled = 1;
    if (contains_line(input, "BACKUP=1")) cfg->backup = 1;
    if (contains_line(input, "METRICS=1")) cfg->metrics = 1;

    if (contains_line(input, "DB=mysql")) strcpy(cfg->db, "mysql");
    if (contains_line(input, "DB=sqlite")) strcpy(cfg->db, "sqlite");

    if (contains_line(input, "ROLE=admin")) strcpy(cfg->role, "admin");
    if (contains_line(input, "ROLE=user")) strcpy(cfg->role, "user");

    if (contains_line(input, "COMPRESS=gzip")) strcpy(cfg->compress, "gzip");
    if (contains_line(input, "COMPRESS=lz4")) strcpy(cfg->compress, "lz4");

    if (contains_line(input, "FORMAT=json")) strcpy(cfg->format, "json");
    if (contains_line(input, "FORMAT=xml")) strcpy(cfg->format, "xml");
}

static void feature_auth(void) {
    volatile int x = 1;
    x += 3;                         // do some work
    record_feature("AUTH");         // register feat
}

static void feature_cache(void) {
    volatile int x = 2;
    x *= 5;
    record_feature("CACHE");
}

static void feature_tls(void) {
    volatile int x = 3;
    x ^= 0x55;
    record_feature("TLS");
}

static void feature_db_mysql(void) {
    volatile int x = 4;
    x += 100;
    record_feature("DB_MYSQL");
}

static void feature_db_sqlite(void) {
    volatile int x = 5;
    x += 200;
    record_feature("DB_SQLITE");
}

static void feature_admin(void) {
    volatile int x = 6;
    x *= 7;
    record_feature("ADMIN");
}

static void feature_backup(void) {
    volatile int x = 7;
    x *= 11;
    record_feature("BACKUP");
}

static void feature_export(void) {
    volatile int x = 8;
    x *= 13;
    record_feature("EXPORT");
}

static void feature_metrics(void) {
    volatile int x = 9;
    x *= 17;
    record_feature("METRICS");
}

static void run_application(Config *cfg) {
    if (cfg->auth) {
        feature_auth();
    }

    if (cfg->cache) {
        feature_cache();
    }

    if (cfg->tls) {
        feature_tls();

        if (cfg->auth) {
            record_dep("TLS", "AUTH");
            record_combo("AUTH", "TLS");
        }
    }

    if (strcmp(cfg->db, "mysql") == 0) {
        feature_db_mysql();
    }

    if (strcmp(cfg->db, "sqlite") == 0) {
        feature_db_sqlite();
    }

    if (strcmp(cfg->role, "admin") == 0) {
        if (cfg->auth) {
            feature_admin();
            record_dep("ADMIN", "AUTH");
            record_combo("AUTH", "ADMIN");
        }
    }

    if (cfg->cache && strcmp(cfg->db, "mysql") == 0) {
        record_combo("CACHE", "DB_MYSQL");
    }

    if (cfg->backup) {
        if (strcmp(cfg->db, "mysql") == 0 || strcmp(cfg->db, "sqlite") == 0) {
            feature_backup();
            record_dep("BACKUP", "DB");
        }
    }

    if (cfg->export_enabled) {
        feature_export();

        if (strcmp(cfg->compress, "gzip") == 0) {
            record_dep("EXPORT", "COMPRESS_GZIP");
            record_combo("EXPORT", "COMPRESS_GZIP");
        }

        if (strcmp(cfg->format, "json") == 0) {
            record_combo("EXPORT", "FORMAT_JSON");
        }
    }

    if (cfg->metrics) {
        feature_metrics();

        if (cfg->cache) {
            record_combo("METRICS", "CACHE");
        }
    }
}

int main(int argc, char **argv) {
    FILE *f;
    char input[MAX_INPUT];
    size_t n;
    Config cfg;

    if (argc < 2) {
        return 1;
    }

    f = fopen(argv[1], "rb");
    if (!f) {
        return 1;
    }

    n = fread(input, 1, MAX_INPUT - 1, f);
    fclose(f);

    input[n] = '\0';

    feedback_init();

    parse_config(input, &cfg);
    run_application(&cfg);

    feedback_close();

    return 0;
}