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
    int begin_tx;
    int write_op;
    int commit;

    int token;
    int size;
    int timeout;
    int threads;
    int checksum;
    int magic;
    int tx_sequence;

    char db[32];
    char role[32];
    char compress[32];
    char format[32];
    char mode[32];
    char region[32];
} Config;

static int parse_int_value(const char *value) {
    char *end = NULL;
    long parsed = strtol(value, &end, 0);

    if (end == value || *end != '\0') {
        return 0;
    }

    if (parsed > 2147483647L) {
        return 2147483647;
    }

    if (parsed < -2147483647L - 1L) {
        return -2147483647 - 1;
    }

    return (int)parsed;
}

static void advance_tx_sequence(Config *cfg, const char *key, const char *value, int *step) {
    switch (*step) {
    case 0:
        if (strcmp(key, "AUTH") == 0 && strcmp(value, "1") == 0) {
            *step = 1;
        }
        break;
    case 1:
        if (strcmp(key, "DB") == 0 && strcmp(value, "mysql") == 0) {
            *step = 2;
        }
        break;
    case 2:
        if (strcmp(key, "BEGIN_TX") == 0 && strcmp(value, "1") == 0) {
            *step = 3;
        }
        break;
    case 3:
        if (strcmp(key, "WRITE") == 0 && strcmp(value, "1") == 0) {
            *step = 4;
        }
        break;
    case 4:
        if (strcmp(key, "COMMIT") == 0 && strcmp(value, "1") == 0) {
            *step = 5;
        }
        break;
    case 5:
        if (strcmp(key, "BACKUP") == 0 && strcmp(value, "1") == 0) {
            cfg->tx_sequence = 1;
            *step = 6;
        }
        break;
    default:
        break;
    }
}

static void apply_kv(Config *cfg, const char *key, const char *value) {
    if (strcmp(key, "AUTH") == 0) cfg->auth = strcmp(value, "1") == 0;
    else if (strcmp(key, "CACHE") == 0) cfg->cache = strcmp(value, "1") == 0;
    else if (strcmp(key, "TLS") == 0) cfg->tls = strcmp(value, "1") == 0;
    else if (strcmp(key, "EXPORT") == 0) cfg->export_enabled = strcmp(value, "1") == 0;
    else if (strcmp(key, "BACKUP") == 0) cfg->backup = strcmp(value, "1") == 0;
    else if (strcmp(key, "METRICS") == 0) cfg->metrics = strcmp(value, "1") == 0;
    else if (strcmp(key, "BEGIN_TX") == 0) cfg->begin_tx = strcmp(value, "1") == 0;
    else if (strcmp(key, "WRITE") == 0) cfg->write_op = strcmp(value, "1") == 0;
    else if (strcmp(key, "COMMIT") == 0) cfg->commit = strcmp(value, "1") == 0;
    else if (strcmp(key, "DB") == 0) {
        if (strcmp(value, "mysql") == 0 || strcmp(value, "sqlite") == 0 || strcmp(value, "none") == 0) {
            snprintf(cfg->db, sizeof(cfg->db), "%s", value);
        }
    } else if (strcmp(key, "ROLE") == 0) {
        if (strcmp(value, "admin") == 0 || strcmp(value, "user") == 0) {
            snprintf(cfg->role, sizeof(cfg->role), "%s", value);
        }
    } else if (strcmp(key, "COMPRESS") == 0) {
        if (strcmp(value, "gzip") == 0 || strcmp(value, "lz4") == 0 || strcmp(value, "none") == 0) {
            snprintf(cfg->compress, sizeof(cfg->compress), "%s", value);
        }
    } else if (strcmp(key, "FORMAT") == 0) {
        if (strcmp(value, "json") == 0 || strcmp(value, "xml") == 0 || strcmp(value, "txt") == 0) {
            snprintf(cfg->format, sizeof(cfg->format), "%s", value);
        }
    } else if (strcmp(key, "MODE") == 0) {
        if (strcmp(value, "write") == 0 || strcmp(value, "read") == 0) {
            snprintf(cfg->mode, sizeof(cfg->mode), "%s", value);
        }
    } else if (strcmp(key, "REGION") == 0) {
        if (strcmp(value, "us-east") == 0 || strcmp(value, "eu-west") == 0 || strcmp(value, "local") == 0) {
            snprintf(cfg->region, sizeof(cfg->region), "%s", value);
        }
    } else if (strcmp(key, "TOKEN") == 0) cfg->token = parse_int_value(value);
    else if (strcmp(key, "SIZE") == 0) cfg->size = parse_int_value(value);
    else if (strcmp(key, "TIMEOUT") == 0) cfg->timeout = parse_int_value(value);
    else if (strcmp(key, "THREADS") == 0) cfg->threads = parse_int_value(value);
    else if (strcmp(key, "CHECKSUM") == 0) cfg->checksum = parse_int_value(value);
    else if (strcmp(key, "MAGIC") == 0) cfg->magic = parse_int_value(value);
}

static void parse_config(const char *input, Config *cfg) {
    char buf[MAX_INPUT];
    char *line;
    char *saveptr = NULL;
    int tx_step = 0;

    memset(cfg, 0, sizeof(Config));

    strcpy(cfg->db, "none");
    strcpy(cfg->role, "user");
    strcpy(cfg->compress, "none");
    strcpy(cfg->format, "txt");
    strcpy(cfg->mode, "read");
    strcpy(cfg->region, "local");

    snprintf(buf, sizeof(buf), "%s", input);

    for (line = strtok_r(buf, "\n\r", &saveptr); line != NULL; line = strtok_r(NULL, "\n\r", &saveptr)) {
        char *eq = strchr(line, '=');

        if (eq == NULL || eq == line || eq[1] == '\0') {
            continue;
        }

        *eq = '\0';
        advance_tx_sequence(cfg, line, eq + 1, &tx_step);
        apply_kv(cfg, line, eq + 1);
    }
}

static int has_db(const Config *cfg) {
    return strcmp(cfg->db, "mysql") == 0 || strcmp(cfg->db, "sqlite") == 0;
}

static int mode_write(const Config *cfg) {
    return strcmp(cfg->mode, "write") == 0;
}

static void feature_auth(const Config *cfg) {
    volatile int x = 1;
    x += 3;
    record_feature("AUTH");

    if (strcmp(cfg->role, "admin") == 0) {
        x += 11;
    } else {
        x += 23;
    }

    if (cfg->tls) {
        volatile int y = 30;
        y ^= 0x55;
    }
}

static void feature_cache(const Config *cfg) {
    volatile int x = 2;
    x *= 5;
    record_feature("CACHE");

    if (strcmp(cfg->db, "mysql") == 0) {
        x += 101;
    } else if (strcmp(cfg->db, "sqlite") == 0) {
        x += 202;
    }
}

static void feature_tls(const Config *cfg) {
    volatile int x = 3;
    x ^= 0x55;
    record_feature("TLS");

    if (cfg->auth && strcmp(cfg->role, "admin") == 0) {
        x ^= 0xaa;
    }
}

static void feature_db_mysql(const Config *cfg) {
    volatile int x = 4;
    x += 100;
    record_feature("DB_MYSQL");

    if (cfg->backup) {
        x += 4096;
    }
}

static void feature_db_sqlite(const Config *cfg) {
    volatile int x = 5;
    x += 200;
    record_feature("DB_SQLITE");

    if (cfg->backup) {
        x += 2048;
    }
}

static void feature_admin(const Config *cfg) {
    volatile int x = 6;
    x *= 7;
    record_feature("ADMIN");

    if (cfg->tls) {
        x ^= 0x777;
    }
}

static void feature_backup(const Config *cfg) {
    volatile int x = 7;
    x *= 11;
    record_feature("BACKUP");

    if (cfg->size > 2048) {
        x += 4096;
    }
}

static void feature_export(const Config *cfg) {
    volatile int x = 8;
    x *= 13;
    record_feature("EXPORT");

    if (strcmp(cfg->compress, "gzip") == 0) {
        x += 31;
    } else if (strcmp(cfg->compress, "lz4") == 0) {
        x += 47;
    }

    if (strcmp(cfg->format, "json") == 0) {
        x += 59;
    } else if (strcmp(cfg->format, "xml") == 0) {
        x += 61;
    }
}

static void feature_metrics(const Config *cfg) {
    volatile int x = 9;
    x *= 17;
    record_feature("METRICS");

    if (cfg->cache) {
        x += 19;
    }
}

static void record_large_export(void) {
    record_feature("LARGE_EXPORT");
    record_combo("EXPORT", "LARGE_EXPORT");
    record_dep("LARGE_EXPORT", "EXPORT");
}

static void record_privileged_admin(void) {
    record_feature("PRIVILEGED_ADMIN");
    record_combo("ADMIN", "PRIVILEGED_ADMIN");
    record_dep("PRIVILEGED_ADMIN", "ADMIN");
}

static void record_query_accel(void) {
    record_feature("QUERY_ACCEL");
    record_combo("CACHE", "QUERY_ACCEL");
    record_combo("DB_MYSQL", "QUERY_ACCEL");
    record_dep("QUERY_ACCEL", "CACHE");
    record_dep("QUERY_ACCEL", "DB_MYSQL");
}

static void record_large_write_backup(void) {
    record_feature("LARGE_WRITE_BACKUP");
    record_combo("BACKUP", "LARGE_WRITE_BACKUP");
    record_dep("LARGE_WRITE_BACKUP", "BACKUP");
}

static void record_failover(void) {
    record_feature("FAILOVER");
    record_combo("TLS", "FAILOVER");
    record_combo("DB_MYSQL", "FAILOVER");
    record_dep("FAILOVER", "TLS");
    record_dep("FAILOVER", "DB_MYSQL");
}

static void record_cluster_sync(void) {
    record_feature("CLUSTER_SYNC");
    record_combo("FAILOVER", "CLUSTER_SYNC");
    record_combo("QUERY_ACCEL", "CLUSTER_SYNC");
    record_dep("CLUSTER_SYNC", "FAILOVER");
    record_dep("CLUSTER_SYNC", "QUERY_ACCEL");
}

static void record_recovery(void) {
    record_feature("RECOVERY");
    record_combo("LARGE_WRITE_BACKUP", "RECOVERY");
    record_combo("CLUSTER_SYNC", "RECOVERY");
    record_dep("RECOVERY", "LARGE_WRITE_BACKUP");
    record_dep("RECOVERY", "CLUSTER_SYNC");
}

static void run_application(Config *cfg) {
    int admin_unlocked = 0;
    int privileged_admin = 0;
    int large_export = 0;
    int query_accel = 0;
    int large_write_backup = 0;
    int failover = 0;
    int cluster_sync = 0;

    if (cfg->auth) {
        feature_auth(cfg);
    }

    if (cfg->cache) {
        feature_cache(cfg);
    }

    if (cfg->tls) {
        feature_tls(cfg);

        if (cfg->auth) {
            record_dep("TLS", "AUTH");
            record_combo("AUTH", "TLS");
        }
    }

    if (strcmp(cfg->db, "mysql") == 0) {
        feature_db_mysql(cfg);
    }

    if (strcmp(cfg->db, "sqlite") == 0) {
        feature_db_sqlite(cfg);
    }

    if (cfg->auth && strcmp(cfg->role, "admin") == 0 && cfg->tls) {
        feature_admin(cfg);
        admin_unlocked = 1;
        record_dep("ADMIN", "AUTH");
        record_dep("ADMIN", "TLS");
        record_combo("AUTH", "ADMIN");
        record_combo("TLS", "ADMIN");
    }

    if (cfg->cache && strcmp(cfg->db, "mysql") == 0 && cfg->threads >= 4) {
        record_combo("CACHE", "DB_MYSQL");
    }

    if (cfg->backup && has_db(cfg) && mode_write(cfg)) {
        feature_backup(cfg);
        record_dep("BACKUP", "DB");
        record_dep("BACKUP", "MODE_WRITE");
    }

    if (cfg->export_enabled && strcmp(cfg->compress, "gzip") == 0 && strcmp(cfg->format, "json") == 0) {
        feature_export(cfg);
        record_dep("EXPORT", "COMPRESS_GZIP");
        record_dep("EXPORT", "FORMAT_JSON");
        record_combo("EXPORT", "COMPRESS_GZIP");
        record_combo("EXPORT", "FORMAT_JSON");
    }

    if (cfg->metrics) {
        feature_metrics(cfg);

        if (cfg->cache && cfg->timeout >= 100) {
            record_combo("METRICS", "CACHE");
        }
    }

    if (admin_unlocked && cfg->token == 1337 && mode_write(cfg) && cfg->checksum == 42) {
        record_privileged_admin();
        privileged_admin = 1;
    }

    if (cfg->export_enabled && strcmp(cfg->compress, "gzip") == 0 && strcmp(cfg->format, "json") == 0 &&
        cfg->size > 2048 && mode_write(cfg)) {
        record_large_export();
        large_export = 1;
    }

    if (cfg->cache && strcmp(cfg->db, "mysql") == 0 && cfg->threads >= 8 && cfg->checksum == 42) {
        record_query_accel();
        query_accel = 1;
    }

    if (cfg->backup && strcmp(cfg->db, "mysql") == 0 && mode_write(cfg) && cfg->size > 2048 && cfg->tx_sequence) {
        record_large_write_backup();
        large_write_backup = 1;
    }

    if (cfg->tls && strcmp(cfg->db, "mysql") == 0 && strcmp(cfg->region, "us-east") == 0 && cfg->timeout >= 100 && cfg->checksum == 42) {
        record_failover();
        failover = 1;
    }

    if (failover && query_accel && cfg->threads >= 8 && cfg->magic == 0xdead) {
        record_cluster_sync();
        cluster_sync = 1;
    }

    if (cluster_sync && large_write_backup && large_export && privileged_admin && cfg->commit && cfg->magic == 0xdead) {
        record_recovery();
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
