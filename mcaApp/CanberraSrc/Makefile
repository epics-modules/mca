TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaCanberra.dbd
DBD += mcaCanberraSupport.dbd

INC_vxWorks += llc.h
INC_Linux   += linux-llc.h
INC         += nmc_sys_defs.h aim_comm_defs.h ncp_comm_defs.h 
INC         += nmcmsgdef.h icb_sys_defs.h ndtypes.h campardef.h

# The configuration here is rather complex, because there is no OS-independent method of sending
# and receiving arbitrary Ethernet packets.  We use different methods depending on the OS and version.
#
#      OS/version        #defines                   Libraries
#
# vxWorks 5.5 and later  USE_SOCKETS, USE_MUXTKLIB  muxTkLib
# vxWorks pre-5.5        USE_SOCKETS                muxLib     because of a bug in muxTkLib)
# Linux 2.6.13 and later USE_SOCKETS                           This requires LLC socket support in kernel
# Linux any version      USE_LIBNET                 libnet, libpcap
# Darwin                 USE_LIBNET                 libnet, libpcap
# Cygwin                 USE_WINPCAP                WinPcap
# WIN32                  USE_WINPCAP                WinPcap


# vxWorks
LIBRARY_IOC_vxWorks      += mcaCanberra
PROD_IOC_vxWorks         += mcaAIM
USR_CFLAGS_vxWorks       += -DUSE_SOCKETS
USR_CPPFLAGS_vxWorks     += -DUSE_SOCKETS
mcaCanberra_SRCS_vxWorks += llcProtocol.c
mcaCanberra_SRCS_vxWorks += llcSocket.c
# On vxWorks 5.5 and later we use muxTkLib, so uncomment the following line.
# On earlier versions there are bugs in muxTkLib and we use muxLib instead.
#USR_CFLAGS_vxWorks += -DUSE_MUXTKLIB

# Linux
# To build for Linux system uncomment the following lines.  This requires libnet or
# LLC sockets
ifeq ($(LINUX_NET_INSTALLED), YES)
LIBRARY_IOC_Linux += mcaCanberra
PROD_IOC_Linux    += mcaAIM
PROD_IOC_Linux    += nmcDemo
endif
# We can only use LLC sockets on Linux versions
# more recent than 2.6.13 due to a show-shopper bug in the LLC socket
# layer that was fixed in 2.6.14. 
#USR_CFLAGS_Linux += -DUSE_SOCKETS
# If LLC sockets are not being used on Linux, and libnet is being used instead, then
# uncomment the following lines.
USR_CFLAGS_Linux           += -DUSE_LIBNET
USR_CPPFLAGS_Linux         += -DUSE_LIBNET
USR_CFLAGS_Linux           += -g `libnet-config --defines` 
USR_CPPFLAGS_Linux         += -g `libnet-config --defines` 
mcaCanberra_SYS_LIBS_Linux += net pcap
mcaAIM_SYS_LIBS_Linux      += net pcap
nmcDemo_SYS_LIBS_Linux     += net pcap

# Darwin
LIBRARY_IOC_Darwin          += mcaCanberra
PROD_IOC_Darwin             += mcaAIM
PROD_IOC_Darwin             += nmcDemo
USR_CFLAGS_Darwin           += -DUSE_LIBNET
USR_CPPFLAGS_Darwin         += -DUSE_LIBNET
USR_CFLAGS_Darwin           += -g `libnet-config --defines` 
USR_CPPFLAGS_Darwin         += -g `libnet-config --defines` 
mcaCanberra_SYS_LIBS_Darwin += net pcap
mcaAIM_SYS_LIBS_Darwin      += net pcap
nmcDemo_SYS_LIBS_Darwin     += net pcap

# Cygwin uses WinPcap
LIBRARY_IOC_cygwin32   += mcaCanberra
PROD_IOC_cygwin32      += mcaAIM
USR_CFLAGS_cygwin32    += -DUSE_WINPCAP
USR_CPPFLAGS_cygwin32  += -DUSE_WINPCAP
LIB_LIBS_cygwin32      += wpcap Packet
PROD_LIBS_cygwin32     += wpcap Packet
LIB_SYS_LIBS_cygwin32  += $(CYGWIN_RPC_LIB)
PROD_SYS_LIBS_cygwin32 += $(CYGWIN_RPC_LIB)

# WIN32 uses WinPcap
LIBRARY_IOC_WIN32   += mcaCanberra
PROD_IOC_WIN32      += mcaAIM
USR_CFLAGS_WIN32    += -DUSE_WINPCAP -DWIN32
USR_CPPFLAGS_WIN32  += -DUSE_WINPCAP -DWIN32
LIB_LIBS_WIN32      += wpcap Packet
PROD_LIBS_WIN32     += wpcap Packet
LIB_SYS_LIBS_WIN32  += ws2_32
PROD_SYS_LIBS_WIN32 += ws2_32

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaCanberra_SRCS += nmc_comm_subs_1.c 
mcaCanberra_SRCS += nmc_comm_subs_2.c 
mcaCanberra_SRCS += nmc_user_subs_1.c
mcaCanberra_SRCS += nmc_user_subs_2.c 
mcaCanberra_SRCS += drvMcaAIMAsyn.c
mcaCanberra_SRCS += icb_strings.c
mcaCanberra_SRCS += icb_crmpsc.c
mcaCanberra_SRCS += icb_handler_subs.c
mcaCanberra_SRCS += icb_control_subs.c
mcaCanberra_SRCS += icb_control_subs2.c
mcaCanberra_SRCS += icb_user_subs.c
mcaCanberra_SRCS += icb_show_modules.c
mcaCanberra_SRCS += drvIcbAsyn.c
mcaCanberra_SRCS += DSA2000.cpp
mcaCanberra_SRCS += mcaCanberraRegister.c

mcaCanberra_LIBS += asyn 
mcaCanberra_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAIM_SRCS         += mcaCanberra_registerRecordDeviceDriver.cpp
mcaAIM_SRCS_DEFAULT += mcaCanberraMain.cpp
mcaAIM_SRCS_vxWorks += -nil-
mcaAIM_LIBS        += mca
mcaAIM_LIBS        += mcaCanberra
mcaAIM_LIBS        += calc autosave
mcaAIM_LIBS        += sscan busy
mcaAIM_LIBS        += asyn
mcaAIM_LIBS        += $(EPICS_BASE_IOC_LIBS)


#=============================
PROD_IOC_vxWorks += nmcTest

nmcTest_LIBS += ca Com 

nmcTest_SRCS += nmc_comm_subs_1.c 
nmcTest_SRCS += nmc_comm_subs_2.c 
nmcTest_SRCS += nmc_user_subs_1.c
nmcTest_SRCS += nmc_user_subs_2.c 
nmcTest_SRCS += nmc_test.c

#=============================
PROD_IOC_vxWorks += nmcDemo

nmcDemo_LIBS += ca Com 

nmcDemo_SRCS += nmc_comm_subs_1.c 
nmcDemo_SRCS += nmc_comm_subs_2.c 
nmcDemo_SRCS += nmc_user_subs_1.c
nmcDemo_SRCS += nmc_user_subs_2.c 
nmcDemo_SRCS += nmc_demo.c


#=============================
PROD_IOC_vxWorks += muxTkTest

muxTkTest_SRCS += muxTkTest.c 


#=============================
PROD_IOC_vxWorks += muxTest

muxTkTest_SRCS += muxTest.c 


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
