TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# libnet needs this
USR_CFLAGS_darwin-ppc += -g `libnet-config --defines` 

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaCanberra.dbd
DBD += mcaCanberraSupport.dbd

INC_vxWorks += llc.h
INC_Linux += linux-llc.h


#=============================
LIBRARY_IOC_vxWorks += mcaCanberra
LIBRARY_IOC_cygwin32 += mcaCanberra
#LIBRARY_IOC_WIN32 += mcaCanberra

USR_CFLAGS_vxWorks += -DUSE_SOCKETS
# On vxWorks 5.5 and later we use muxTkLib, so uncomment the following line.
# On earlier versions there are bugs in muxTkLib and we use muxLib instead.
#USR_CFLAGS_vxWorks += -DUSE_MUXTKLIB

# To build for Linux system uncomment the following lines.  This requires libnet or
# LLC sockets
LIBRARY_IOC_Linux += mcaCanberra
PROD_IOC_Linux += mcaAIM
PROD_IOC_Linux += nmcDemo

# We can only use LLC sockets on Linux versions
# more recent than 2.6.13 due to a show-shopper bug in the LLC socket
# layer that was fixed in 2.6.14. 
#USR_CFLAGS_Linux += -DUSE_SOCKETS
# If LLC sockets are not being used on Linux, and libnet is being used instead, then
# uncomment the following lines.
USR_CFLAGS_linux-x86 += -g `libnet-config --defines` 
mcaCanberra_SYS_LIBS_Linux += net pcap
mcaAIM_SYS_LIBS_Linux      += net pcap
nmcDemo_SYS_LIBS_Linux     += net pcap

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
mcaCanberra_SRCS += mcaCanberraRegister.c
mcaCanberra_SRCS_vxWorks += llcProtocol.c
mcaCanberra_SRCS_vxWorks += llcSocket.c

mcaCanberra_LIBS += asyn 
mcaCanberra_LIBS += $(EPICS_BASE_IOC_LIBS)
mcaCanberra_SYS_LIBS_cygwin32 += wpcap Libnet rpc
mcaCanberra_SYS_LIBS_WIN32 += wpcap Libnet rpc

LIB_INSTALLS_cygwin32 += ../Libnet.lib
LIB_INSTALLS_cygwin32 += ../libwpcap.a
BIN_INSTALLS_cygwin32 += ../Libnet.dll

#==================================
PROD_IOC_vxWorks += mcaAIM
PROD_IOC_cygwin32 += mcaAIM
#PROD_IOC_WIN32 += mcaAIM

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAIM_SRCS += mcaCanberra_registerRecordDeviceDriver.cpp
mcaAIM_SRCS_DEFAULT += mcaCanberraMain.cpp
mcaAIM_SRCS_vxWorks += -nil-
mcaAIM_LIBS += mca
mcaAIM_LIBS += mcaCanberra
mcaAIM_LIBS += calc autosave
mcaAIM_LIBS += busy
mcaAIM_LIBS += asyn
mcaAIM_LIBS += $(EPICS_BASE_IOC_LIBS)

mcaAIM_SYS_LIBS_cygwin32 += wpcap Libnet rpc
mcaAIM_SYS_LIBS_WIN32 += wpcap Libnet rpc

#===========================
# These lines force make to install Libnet and wpcap BEFORE building mcaAIM.exe for Cygwin.
ifneq ($(strip $(LIB_INSTALLS_$(OS_CLASS))),)
LIB_INSTALLS+=$(subst -nil-,,$(LIB_INSTALLS_$(OS_CLASS)))
mcaAIM$(EXE):libInstalls
else
ifdef LIB_INSTALLS_DEFAULT
LIB_INSTALLS+=$(LIB_INSTALLS_DEFAULT)
endif
endif

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
