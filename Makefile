rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

CC = cc
LD = cc
AR = ar

CFLAGS = -std=c11 -Werror -Wextra -Wall -Wimplicit -Wstrict-aliasing \
		 -Iinclude
CFLAGS += -g -O3
CFLAGS += $(shell pkg-config --cflags sdl2)

LDFLAGS = -Llib
LDFLAGS += -g -flto -O3

LIBSRCS = $(call rwildcard,src/libgoodboy,*.c)
LIBOBJS = $(LIBSRCS:.c=.o)
LIBDEPS = $(LIBSRCS:.c=.d)

GBSRCS = $(call rwildcard,src/goodboy,*.c)
GBOBJS = $(GBSRCS:.c=.o)
GBDEPS = $(GBSRCS:.c=.d)

TSTSRCS = $(call rwildcard,tst,*.c)
TSTOBJS = $(TSTSRCS:.c=.o)
TSTDEPS = $(TSTSRCS:.c=.d)
TSTEXES = $(TSTSRCS:.c=.tst)

.PHONY: all test clean
.PRECIOUS: $(TSTOBJS) $(TSTDEPS) $(TSTEXES)

all: bin/goodboy test

lib/libgoodboy.a: $(LIBDEPS) $(LIBOBJS)
	$(AR) rcs $@ $(LIBOBJS)

bin/goodboy: lib/libgoodboy.a $(GBDEPS) $(GBOBJS)
	$(LD) $(GBOBJS) -o $@ $(LDFLAGS) -lgoodboy $(shell pkg-config --libs sdl2)

%.tst: %.o %.d lib/libgoodboy.a
	$(LD) $< -o $@ $(LDFLAGS) -lgoodboy
	@echo -n "$@..."
	@$@
	@echo " OK"

%.o %.d: %.c
	$(CC) $(CFLAGS) -MD -MF $(addsuffix .d,$(basename $<)) -c $< -o $(addsuffix .o,$(basename $<))

test: $(TSTEXES)

clean:
	rm -f bin/*
	rm -f lib/*
	rm -f $(call rwildcard,src,*.o)
	rm -f $(call rwildcard,src,*.d)
	rm -f $(call rwildcard,tst,*.o)
	rm -f $(call rwildcard,tst,*.d)
	rm -f $(call rwildcard,tst,*.tst)


ifneq ($(MAKECMDGOALS),clean)
include $(GBDEPS)
include $(LIBDEPS)
endif

