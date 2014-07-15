git clone https://github.com/bas-t/tbs6281-5-intree.git && cd tbs6281-5-intree

apt-get source linux && cd linux*

mkdir drivers/media/pci/saa716x

cp -f ../drivers/media/pci/saa716x/* drivers/media/pci/saa716x/

cp -f ../drivers/media/dvb-frontends/* drivers/media/dvb-frontends/

cp -f ../drivers/media/tuners/* drivers/media/tuners/

sed -i "s/saa7146\//saa7146\/        \\\/" drivers/media/pci/Makefile

sed -i '/saa7146\/        \\/a saa716x\/' drivers/media/pci/Makefile

sed -i "\/source \"drivers\/media\/pci\/ddbridge\/Kconfig\"/a\
\source \"drivers\/media\/pci\/saa716x\/Kconfig\"" drivers/media/pci/Kconfig

patch -p0 < ../Kconfig.patch

patch -p0 < ../Kconfig-1.patch

sed -i "$ a\
obj-\$(CONFIG_DVB_SI2168) += si2168.o" drivers/media/dvb-frontends/Makefile

sed -i "$ a\
obj-\$(CONFIG_MEDIA_TUNER_SI2157) += si2157.o" drivers/media/tuners/Makefile

patch -p1 < ../3.13-dvb-mutex.patch

_ABINAME=$(cat debian/config/defines | grep 'abiname:' | awk -F"[ ]" '{ print $NF }')

export _ABINAME

sed -i "s/abiname: $_ABINAME/abiname: 90/" debian/config/defines

sed -i "s/ABINAME_PART='-$_ABINAME'/ABINAME_PART='-90'/g" debian/rules.gen

fakeroot debian/rules debian/control-real




sed -i "s/DEBUG='True'/DEBUG='FALSE'/g" debian/rules.gen

fakeroot make -f debian/rules.gen setup_amd64_none_amd64

make -C debian/build/build_amd64_none_amd64 menuconfig

fakeroot make -f debian/rules.gen binary-arch_amd64_none_real

fakeroot make -f debian/rules.gen binary-arch_amd64_none_amd64


