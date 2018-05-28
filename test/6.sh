#!/bin/bash

#QEMU=kvm
QEMU=qemu-system-i386

make test-sleep2 networkenabled initrd

(
	sleep 1
	$QEMU -kernel bin/kernel -initrd bin/initrd.tar -serial `cat tmp/pts-1.txt` &
	$QEMU -kernel bin/kernel -initrd bin/initrd.tar -serial `cat tmp/pts-2.txt` &
	$QEMU -kernel bin/kernel -initrd bin/initrd.tar -serial `cat tmp/pts-3.txt` &
	wait
) &

bin/networkhelper 3 MigrationTest-1 0

echo "NetworkHelper sonlandi, cikis icin ctrl-c'ye basin"
wait
