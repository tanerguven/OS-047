D = ../..
include $(D)/Makefile.in

NEWLIB_VER=1.20.0

CFLAGS = $(M) -nostdlib -fno-builtin -fno-stack-protector -O1 -Wall \
	-Inewlib-${NEWLIB_VER}/newlib/libc/include

OBJS = memcpy.o \
	memmove.o \
	memset.o \
	strcat.o \
	strcmp.o \
	strcpy.o \
	strlen.o \
	strncat.o \
	strncmp.o \
	strncpy.o \
	strnlen.o \
	memcmp.o \
	strchr.o \
	strrchr.o

all:
	@

download:
	@if [ ! -d newlib-${NEWLIB_VER} ]; \
	then \
		if [ ! -f newlib-${NEWLIB_VER}.tar.gz ]; \
		then \
			wget -c "http://sources.redhat.com/pub/newlib/newlib-${NEWLIB_VER}.tar.gz"; \
		fi; \
		tar xf newlib-${NEWLIB_VER}.tar.gz; \
		cp newlib-1.20.0/newlib/libc/string/*.c .; \
	fi

compile: $(OBJS)
	@echo [ar] string.a
	@ar cr string.a $(OBJS)
	@echo [ld] string.O
	@$(LD) -r -o string.O $(OBJS)

clean:
	rm -rf *.o *.O *.c *.d *.a newlib-${NEWLIB_VER}

clean-all: clean
	rm -rf newlib-${NEWLIB_VER}*
