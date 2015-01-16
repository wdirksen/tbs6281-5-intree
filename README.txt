##
## HOW-TO
##
## For use with linux-3.19 and up
##

rm -rf saa716x-intree

git clone --depth=1 https://github.com/bas-t/saa716x-intree.git -b master && cd saa716x-intree

##
## Get and prepare the kernel source
##
## wget <some 3.19 or up kernel>
##

wget https://www.kernel.org/pub/linux/kernel/v3.x/testing/linux-3.19-rc4.tar.xz

tar -xJf linux* && cd linux*

make clean && make mrproper

##
## Add TBS 6281/6285 open source drivers to the kernel source
## That is, for the saa716x pcie bridge chip. Demod and tuner are in linux-3.19
##

mkdir drivers/media/pci/saa716x

cp -f ../drivers/media/pci/saa716x/* drivers/media/pci/saa716x/

patch -p0 < ../Kconfig.patch

patch -p0 < ../Makefile.patch

patch -p0 < ../silabs-3.19.patch

patch -p0 < ../uapi.patch


##
## Patch dvbdev.c for use with FFdecsawrapper
##

patch -p0 < ../dvb-core.patch

##
## Configure the kernel
##

make oldconfig

make menuconfig

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




