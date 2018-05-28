D = ..
include ../Makefile.in

CFLAGS = $(M) -I$(D)/include -Iinclude -O0 -g3 -Wall -c -nostdlib -nostdinc -fno-builtin\
	    -fno-stack-protector -nostartfiles -nodefaultlibs -D_USER_PROGRAM

LIB = \
	lib/syscall.o \
	lib/stdio.o \
	lib/vsprintf.o

PROGRAMS = \
	program/hello \
	program/loop \
	program/loopb \
	program/loopc \
	program/loopd \
	program/emptyloop \
	program/sleep \
	program/sleep2 \
	program/forktest \
	program/exectest \
	program/exittest

all: lib/lib.a $(PROGRAMS)
	@

lib/lib.a: $(LIB)
	@ar r lib/lib.a $(LIB)

LIBS = \
	lib/lib.a \
	$(D)/lib/str/string.a

program/%: program/%.o
	@echo [ld] program/$*
	@$(LD) -T"link.ld" -o $(INITRD_BIN_FOLDER)/$* program/$*.o $(LIBS)

clean:
	rm -f */*.d */*.o
	@rm -f lib/lib.a
