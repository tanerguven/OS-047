D = .
include $(D)/Makefile.in

######################################
#	env variables
######################################

ifndef QEMU
QEMU := qemu-system-i386
endif

######################################
#
######################################

INCLUDE_DIRS = \
	kernel/Makefile.in \
	lib/Makefile.in

include $(INCLUDE_DIRS)

OBJS := $(SRC:.S=.o)
OBJS := $(OBJS:.c=.o)
DEPS = $(OBJS:.o=.d)

SRC += tools/networkprotocol.c

all: kernel build-user initrd networkhelper

-include $(DEPS)

kernel: $(OBJS) lib/str/string.O
	@mkdir -p bin
	@echo [$(LD)] bin/kernel
	@$(LD) -T link.ld -o bin/kernel $(OBJS) lib/str/string.O
	@objdump -S bin/kernel > bin/kernel.asm

build-user:
	@mkdir -p bin/initrd
	@cd user; make clean all

initrd:
	@mkdir -p bin/initrd
	@cd bin; tar cf initrd.tar initrd

lib/str/string.O:
	@cd lib/str; make download; make compile;

clean-lib-str:
	cd lib/str; make clean;

clean:
	cd user; make clean
	find . -name "*.o" | xargs rm -f
	find . -name "*.d" | xargs rm -f

# qemu-nographic: kernel
# 	mkdir -p tmp/
# 	$(QEMU) -kernel bin/kernel -initrd bin/initrd.tar -nographic -serial mon:stdio

networkdisabled:
	rm -f bin/initrd/NetworkEnabled

networkenabled:
	touch bin/initrd/NetworkEnabled

test-%:
	echo $* > bin/initrd/init.txt

qemu: networkdisabled initrd kernel
	$(QEMU) -kernel bin/kernel -initrd bin/initrd.tar

qemu-gdb: networkdisabled initrd kernel
	$(QEMU) -kernel bin/kernel -initrd bin/initrd.tar -serial mon:stdio -S -s

networkhelper:
	mkdir -p tmp/ bin/
	gcc -lpthread -Wall tools/networkhelper.c tools/networkprotocol.c -o bin/networkhelper

gdb:
	gdb --tui --command=gdb-config.txt
