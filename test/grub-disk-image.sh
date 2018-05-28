#!/bin/bash

if [ "${KODU_KONTROL_ETTIM}" != "y" ]; then
	echo "UYARI: Bu script, fdisk kullandigi icin tehlikelidir."
	echo "  Koddaki adimlari el ile uygulamaniz onerilir."
	echo "  Scripti yine de kullanmak istiyorsaniz: KODU_KONTROL_ETTIM=y $0 seklinde calistirin."
	exit
fi

if [ "$(id -u)" != "0" ]; then
	echo "Bu scriptin root olarak calistirilmasi gerekir"
	exit
fi

CURDIR=`pwd`

DISK_IMG=$CURDIR/bin/disk.img
KERNEL=$CURDIR/bin/kernel
INITRD=$CURDIR/bin/initrd.tar

dd if=/dev/zero of=$DISK_IMG bs=1M count=32

(
	echo o # Create a new empty DOS partition table
	echo n # Add a new partition
	echo p # Primary partition
	echo 1 # Partition number
	echo   # First sector (Accept default: 1)
	echo   # Last sector (Accept default: varies)
	echo w # Write changes
) | fdisk $DISK_IMG


mkdir -p $CURDIR/mnt/
LOOPDEV=$(losetup --find --show $DISK_IMG)
echo $LOOPDEV
partx -av $LOOPDEV
mkfs.ext2 ${LOOPDEV}p1
mount ${LOOPDEV}p1 $CURDIR/mnt/
grub-install --root-directory=$CURDIR/mnt/ --no-floppy --modules="normal part_msdos ext2 multiboot" ${LOOPDEV}

cp $KERNEL $CURDIR/mnt/boot/OS047
cp $INITRD $CURDIR/mnt/boot/OS047-initrd

echo 'menuentry 'OS-047' {
echo 'Loading OS-047  ...'
multiboot /boot/OS047
echo 'Loading initial ramdisk ...'
module /boot/OS047-initrd
}' > $CURDIR/mnt/boot/grub/grub.cfg

ls $CURDIR/mnt/boot
umount $CURDIR/mnt/

partx -d $LOOPDEV
losetup -d $LOOPDEV

echo
echo "gercek bilgisayarda calistirmak icin bin/disk.img dosyasini USB bellege dd ile yazdirabilirsiniz"
