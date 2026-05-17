CC = afl-clang-fast
CFLAGS ?= -O0 -g -Wall -Wextra
CPPFLAGS ?=

.PHONY: all clean feedback

all: configlet_sim

configlet_sim: src/configlet_sim.c src/feedback.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o configlet_sim src/configlet_sim.c

feedback: CPPFLAGS += -DCONFIGLET_FEEDBACK
feedback: clean configlet_sim

clean:
	rm -f configlet_sim
