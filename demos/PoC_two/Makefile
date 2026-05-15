CC=afl-clang-fast
CFLAGS=-O0 -g -Wall -Wextra

all: configlet_sim

configlet_sim: src/configlet_sim.c src/feedback.h
	$(CC) $(CFLAGS) -o configlet_sim src/configlet_sim.c

clean:
	rm -f configlet_sim