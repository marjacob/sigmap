SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .o

bindir = bin
objdir = obj
srcdir = src
bin = $(bindir)/sigmap

CFLAGS = \
	-c \
	-fstack-protector \
	-fvisibility=hidden \
	-I$(srcdir) \
	-pipe \
	-Wall \
	-Werror \
	-Wformat-nonliteral \
	-Wformat-security \
	-Winit-self \
#	-Wl,-z,now \
#	-Wl,-z,relro \
	-Wpointer-arith
LDFLAGS = \
	-rdynamic \
#	-Wl,--build-id=sha1 \
	-Wl,--discard-all \
	-Wl,--no-undefined \
	-Wl,-O1

ALL_CFLAGS = -std=gnu99 -Isrc/include $(CFLAGS)
ALL_LDFLAGS = -lpthread $(LDFLAGS)

objects = \
	$(objdir)/critical.o \
	$(objdir)/main.o \
	$(objdir)/utils.o

all: $(bin)
$(objdir)/critical: $(srcdir)/critical.c
$(objdir)/main: $(srcdir)/main.c
$(objdir)/utils.o: $(srcdir)/utils.c

clean:
	@$(RM) -rf "$(objdir)" "$(bindir)"

debug: CFLAGS += -g -DDEBUG
debug: all

$(bin): $(objects)
	@printf "linking executable %s\n" "$@"
	@mkdir -p $(@D)
	@$(CC) $(ALL_LDFLAGS) -o $@ $^

$(objdir)/%.o: $(srcdir)/%.c Makefile
	@printf "building object %s\n" "$@"
	@mkdir -p $(@D)
	@$(CC) -c $(ALL_CFLAGS) $< -o $@
