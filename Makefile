# mcmap/Makefile

sources := cmd.c main.c map.c protocol.c world.c
libs := gio-2.0 sdl zlib

objs := $(sources:.c=.o)
deps := $(sources:.c=.d)

CC := gcc

CFLAGS := $(CFLAGS)
CFLAGS += -Wall -Werror -std=gnu99
CFLAGS += $(shell pkg-config --cflags $(libs))

ifndef EXTCFLAGS
	ifdef DEBUG
		EXTCFLAGS := -g
	else
		EXTCFLAGS := -O3 -combine -funroll-loops -fwhole-program
	endif
endif

LDFLAGS := $(LDFLAGS)
LDFLAGS += $(shell pkg-config --libs $(libs))
LDFLAGS += -lreadline

.PHONY: all diet debug clean

all: mcmap

debug:
	@$(MAKE) --no-print-directory DEBUG=1

diet:
	@$(MAKE) --no-print-directory CC="diet -Os $(CC)" EXTCFLAGS=""

ifdef DEBUG
	-include $(deps)
endif

ifdef DEBUG
mcmap: $(objs)
	$(CC) -o $@ $^ $(LDFLAGS)
else
mcmap: $(sources)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(sources)
endif

clean:
	$(RM) mcmap $(objs) $(deps)

%.d: %.c
	@set -e; rm -f $@; \
	 $(CC) -MM $(CFLAGS) $< | sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@
