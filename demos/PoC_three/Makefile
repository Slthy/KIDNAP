CC ?= afl-clang-fast
CFLAGS ?= -O0 -g -Wall -Wextra -std=c11
CPPFLAGS ?= -Iharness

.PHONY: all clean

all: systemctl_generator systemctl_harness unit_file_harness

systemctl_generator: harness/systemctl_generator.c harness/input_parser.c harness/input_parser.h harness/safe_allowlist.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ harness/systemctl_generator.c harness/input_parser.c

systemctl_harness: harness/systemctl_harness.c harness/input_parser.c harness/input_parser.h harness/safe_allowlist.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ harness/systemctl_harness.c harness/input_parser.c

unit_file_harness: harness/unit_file_harness.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ harness/unit_file_harness.c

clean:
	rm -f systemctl_generator systemctl_harness unit_file_harness
