#!/bin/bash

if [ $# -ne 1 ]
then
    echo "### Need to pass an argument ###"
    exit
fi

if [ "$1" == "--clean" ]
then
    echo "### Cleaning... ###"
    rm -f *.o *.aplx *.elf *.txt *~ *.ybug
    exit
fi    

make APP=$1

tubotron & disown

BOOTFILE=boot.ybug
echo "### BOOTING ###" > $BOOTFILE
echo "boot scamp.boot spin3.conf" >> $BOOTFILE
echo "sleep 1" >> $BOOTFILE
echo "iptag . 17894 4" >> $BOOTFILE
echo "sleep 1" >> $BOOTFILE
echo "app_load $1.aplx 1 16" >> $BOOTFILE
echo "ps" >> $BOOTFILE

SPINNAKER="192.168.240.37"

ybug $SPINNAKER << EOF
@ ./$BOOTFILE
EOF

ybug $SPINNAKER
