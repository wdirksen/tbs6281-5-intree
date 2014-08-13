##
## HOW-TO
##


git clone https://github.com/bas-t/tbs6281-5-intree.git && cd tbs6281-5-intree

##
## Get and prepare the kernel source
##
## wget <some kernel>
##

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.15.6.tar.xz

tar -xJf *.xz && cd linux*

make clean && make mrproper

##
## Add TBS 6281/6285 open source drivers to the kernel source
##

mkdir drivers/media/pci/saa716x

cp -f ../drivers/media/pci/saa716x/* drivers/media/pci/saa716x/

cp -f ../drivers/media/dvb-frontends/* drivers/media/dvb-frontends/

cp -f ../drivers/media/tuners/* drivers/media/tuners/

sed -i "s/saa7146\//saa7146\/        \\\/" drivers/media/pci/Makefile

sed -i '/saa7146/a\\t\tsaa716x/' drivers/media/pci/Makefile

sed -i "\/source \"drivers\/media\/pci\/ddbridge\/Kconfig\"/a\\source \"drivers\/media\/pci\/saa716x\/Kconfig\"" drivers/media/pci/Kconfig

patch -p0 < ../Kconfig.patch

patch -p0 < ../Kconfig-1.patch

sed -i "$ a\
obj-\$(CONFIG_DVB_SI2168) += si2168.o" drivers/media/dvb-frontends/Makefile

sed -i "$ a\
obj-\$(CONFIG_MEDIA_TUNER_SI2157) += si2157.o" drivers/media/tuners/Makefile

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


