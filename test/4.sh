#!/bin/bash

#QEMU=kvm
QEMU=qemu-system-i386

make test-loopd networkenabled initrd

(
	sleep 1
	$QEMU -kernel bin/kernel -initrd bin/initrd.tar -serial `cat tmp/pts-1.txt` &
	$QEMU -kernel bin/kernel -initrd bin/initrd.tar -serial `cat tmp/pts-2.txt` &
	wait
) &

bin/networkhelper 2 MigrationTest-1

echo "NetworkHelper sonlandi, cikis icin ctrl-c'ye basin"
wait
