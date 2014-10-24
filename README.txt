##
## HOW-TO
##


git clone https://github.com/bas-t/tbs6281-5-intree.git && cd tbs6281-5-intree

##
## Get and prepare the kernel source
##
## wget <some kernel>
##

wget https://www.kernel.org/pub/linux/kernel/v3.x/testing/linux-3.18-rc1.tar.xz

tar -xJf *.xz && cd linux*

make clean && make mrproper

##
## Add TBS 6281/6285 open source drivers to the kernel source
##

mkdir drivers/media/pci/saa716x

cp -f ../drivers/media/pci/saa716x/* drivers/media/pci/saa716x/

patch -p0 < ../Kconfig.patch

patch -p0 < ../Kconfig-1.patch

patch -p0 < ../Kconfig-2.patch

patch -p0 < ../Makefile.patch

##
## Patch dvbdev.c for use with FFdecsawrapper
##

patch -p1 < ../3.13-dvb-mutex.patch

##
## Configure the kernel
##

make oldconfig

make menuconfig

##
## In an ideal world, the adapters should appear as both DVB-T and DVB-C
## In some cases they only show as DVB-T. That is bad news for those who want
## to use their adapers in DVB-C mode. The good news is: it can be forced to
## DVB-C mode. Drawback: If you force them to DVB-C, DVB-T/T2 is unavailable.
##

patch -p0 < ../force-dvb-c.patch

##
## Compile the kernel (Debian style)
##

make-kpkg --rootcmd fakeroot clean

export CONCURRENCY_LEVEL=3

make-kpkg --rootcmd fakeroot --initrd kernel_image

##
## Install the kernel
##


cd ..

dpkg -i *.deb

##
## Install the firmware
##

cp -f firmware/* /lib/firmware/


