TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC_cygwin32 += pcap.h pcap-bpf.h bpf.h Packet32.h
INC_WIN32    += pcap.h pcap-bpf.h bpf.h Packet32.h pcap-stdinc.h bittypes.h ip6_misc.h

ifeq (win32-x86, $(findstring win32-x86, $(T_A)))
LIB_INSTALLS_WIN32    += ../os/WIN32/wpcap.lib ../os/WIN32/Packet.lib
else ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
LIB_INSTALLS_WIN32    += ../os/WIN32/x64/wpcap.lib ../os/WIN32/x64/Packet.lib
else ifeq (cygwin-x86, $(findstring cygwin-x86, $(T_A)))
LIB_INSTALLS_cygwin32    += ../os/WIN32/wpcap.lib ../os/WIN32/Packet.lib
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
