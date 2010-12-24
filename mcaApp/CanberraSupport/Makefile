TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC_cygwin32 += pcap.h pcap-bpf.h bpf.h Packet32.h
INC_WIN32    += pcap.h pcap-bpf.h bpf.h Packet32.h pcap-stdinc.h bittypes.h ip6_misc.h

LIB_INSTALLS_cygwin32 += ../wpcap.lib ../Packet.lib
BIN_INSTALLS_cygwin32 += ../wpcap.dll ../Packet.dll
LIB_INSTALLS_WIN32    += ../wpcap.lib ../Packet.lib
BIN_INSTALLS_WIN32    += ../wpcap.dll ../Packet.dll

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
