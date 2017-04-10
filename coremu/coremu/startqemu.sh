#!/bin/bash - 
if [ $# != 1 ];then
	echo "./startqemu.sh <num_of_cores>"
	exit 1
fi
sudo obj/qemu/x86_64-softmmu/qemu-system-x86_64 -smp $1 -net none \
	-hda ../img/debian.img -m 4096 -k en-us -nographic -bios bios.bin \
	-snapshot -serial mon:/dev/tty
