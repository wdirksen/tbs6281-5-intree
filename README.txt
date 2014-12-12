##
## HOW-TO
##
## For use with linux-3.18 and up
##


git clone https://github.com/bas-t/tbs6281-5-intree.git && cd tbs6281-5-intree

##
## Get and prepare the kernel source
##
## wget <some kernel>
##

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.18.tar.xz

tar -xJf *.xz && cd linux*

make clean && make mrproper

##
## Add TBS 6281/6285 open source drivers to the kernel source
## That is, for the saa716x pcie bridge chip. Demod and tuner are in linux-3.18
##

mkdir drivers/media/pci/saa716x

cp -f ../drivers/media/pci/saa716x/* drivers/media/pci/saa716x/

patch -p0 < ../Kconfig.patch

patch -p0 < ../Makefile.patch

patch -p0 < ../uapi.patch

patch -p0 < ../silabs.patch

##
## I backported cx23885 changes from linux-next to support my
## DVBSky T982 adapters (other recently added adapters might work too)
##
## This is only for the very impatient, it will be in linux 3.19 !!!
## To use it in a 3.18 kernel, do:
##

patch -p0 < ../cx23885.patch

##
## Patch dvbdev.c for use with FFdecsawrapper
##

patch -p0 < ../dvb-core.patch

##
## Configure the kernel one way:
##

make oldconfig

make menuconfig

##
## or the other (amd64, for Debian Wheezy and up, max 16 DVB adapters):
##

cp -f ../kernelconfig/amd64/3.18-rc6/config ./.config

##
## In an ideal world, the adapters should appear as both DVB-T/T2 and DVB-C
## Current TVHeadend git repo is known to do that in Debian Jessie.
## However, in some cases they only show as DVB-T, since progs like MythTV
## cannot handle multistandard adapers (might not be the only issue).
## That is bad news for those who want to use their adapers in DVB-C mode.
## The good news is: it can be forced to DVB-C mode. Drawback: If you force
## them to DVB-C, DVB-T/T2 is unavailable. Well, I guess you cant't use them
## in both DVB-C and DVB-T mode at the same time anyway.
##

sed -i 's/SYS_DVBT, SYS_DVBT2, SYS_DVBC_ANNEX_A/SYS_DVBC_ANNEX_A/' drivers/media/dvb-frontends/si2168.c

##
## Compile the kernel (Debian style)
##

make-kpkg --rootcmd fakeroot clean

export CONCURRENCY_LEVEL=3

make-kpkg --rootcmd fakeroot --initrd kernel_image > ../build.log

##
## Install the kernel
##


cd ..

dpkg -i *.deb

##
## Install the firmware
##

cp -f firmware/* /lib/firmware/

##
## Set msi mode for saa716x based TBS adapters.
##

echo "options saa716x_budget int_type=1" > /etc/modprobe.d/tbs_opensource.conf




