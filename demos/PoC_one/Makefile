CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
LDLIBS ?=

TARGET := poc_one
SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

.PHONY: all clean sysfeedback

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/%.o: src/%.c include/*.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

sysfeedback: CPPFLAGS += -DSYSCALL_FEEDBACK
sysfeedback: clean $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS)
