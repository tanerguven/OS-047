gerekli programlar
==================

gerekli programlar ve test edilen versiyonlar:
  - gcc : 6.3
  - binutils : 2.28
  - qemu : 2.8.1
  - wget (ilk derleme icin gerekli)


ilk derleme
===========

  make all

Ilk derlemede internetten otomatik olarak newlib kutuphanesi indirilir.


calistirma
==========

  make qemu

veya kvm ile calistirmak icin:

  make qemu QEMU=kvm


ornek program calistirma
========================

  make test-forktest qemu

ornek programlar user/program dizininde. Ornek testler:
  - hello
  - loop
  - forktest
  - exectest


testleri calistirma
===================

  bash test/3.sh

test listesi:
  - test/3.sh : iki makinede toplam 1 process calisirken migration
  - test/4.sh : iki makinede 1 ve 2 process varken 2. processin migrationu
  - test/5.sh : 3.sh ile ayni, sadece program baslangicta klavyeden girdi istiyor
  - test/6.sh : uc makinede toplam 1 process calisirken migration


yeni program yazma, derleme ve calistirma
=========================================

  cd user

  echo '#include <stdio.h>
int main() {
  printf("ilk program!!!\n");
  return 0;
}' > ornek.c

  ./compiler.sh ornek ornek.c

  cd ../

  mv user/ornek bin/initrd

  make test-ornek qemu


gercek bilgisayarda calistirma
==============================

test/grub-disk-image.sh dosyasini inceleyiniz.
