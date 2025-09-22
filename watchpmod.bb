SUMMARY = "Hardware watchpoint kernel module"
DESCRIPTION = "Kernel module that sets hardware breakpoints on a given address and prints backtrace"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHECKSUM = "file://watchpmod.c;beginline=1;endline=5;md5=7661dfd8c7115eeb1f90a0921976bb1c"

SRC_URI = "file://watchpmod.c \
           file://Makefile"

S = "${UNPACKDIR}"

inherit module
