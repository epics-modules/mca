TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# libnet needs this with linux
USR_CFLAGS_linux-x86 += -g `libnet-config --defines` 
USR_CXXFLAGS_linux-x86 += -g `libnet-config --defines`

#=============================

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += mcaRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaAIM.dbd

#=============================
PROD_IOC += mcaAIM

mpf_DIR = $(MPF_LIB)
#synaRec_DIR = $(INSTALL_LIB)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAIM_SRCS += mcaAIM_registerRecordDeviceDriver.cpp
mcaAIM_SRCS_DEFAULT += mcaAIMMain.cpp
mcaAIM_SRCS_vxWorks += -nil-
mcaAIM_SRCS += nmc_comm_subs_1.c 
mcaAIM_SRCS += nmc_comm_subs_2.c 
mcaAIM_SRCS += nmc_user_subs_1.c
mcaAIM_SRCS += nmc_user_subs_2.c 
mcaAIM_SRCS += mcaAIMServer.cc
mcaAIM_SRCS += mcaRecord.c
mcaAIM_SRCS += devMCA_soft.c
mcaAIM_SRCS += devMcaMpf.cc
mcaAIM_SRCS += icb_strings.c
mcaAIM_SRCS += icb_crmpsc.c
mcaAIM_SRCS += icb_handler_subs.c
mcaAIM_SRCS += icb_control_subs.c
mcaAIM_SRCS += icb_control_subs2.c
mcaAIM_SRCS += icb_user_subs.c
mcaAIM_SRCS += icb_show_modules.c
mcaAIM_SRCS += icbServer.cc
mcaAIM_SRCS += devIcbMpf.cc
mcaAIM_SRCS += icbTcaServer.cc
mcaAIM_SRCS += icbDspServer.cc
mcaAIM_SRCS += fastSweep.cc

OBJS += aimRegister
INC += fastSweep.h

#mcaAIM_LIBS += synaRec
mcaAIM_LIBS += mpf mpfEpics std
mcaAIM_LIBS += $(EPICS_BASE_IOC_LIBS)

# should be mcaAIM_LIBS_linux-x86 which won't work
mcaAIM_SYS_LIBS_DEFAULT += net pcap
mcaAIM_SYS_LIBS_vxWorks += -nil-

mcaAIM_LDOBJS_DEFAULT += aimRegister.o
#mcaAIM_LDOBJS_vxWorks += -nil-

#=============================
PROD_IOC += nmcTest

nmcTest_LIBS += ca Com 
nmcTest_SYS_LIBS_DEFAULT += net pcap
nmcTest_SYS_LIBS_vxWorks += -nil-

nmcTest_SRCS += nmc_comm_subs_1.c 
nmcTest_SRCS += nmc_comm_subs_2.c 
nmcTest_SRCS += nmc_user_subs_1.c
nmcTest_SRCS += nmc_user_subs_2.c 
nmcTest_SRCS += nmc_test.c

#=============================
PROD_IOC += nmcDemo

nmcDemo_LIBS += ca Com 
nmcDemo_SYS_LIBS_DEFAULT += net pcap
nmcDemo_SYS_LIBS_vxWorks += -nil-

nmcDemo_SRCS += nmc_comm_subs_1.c 
nmcDemo_SRCS += nmc_comm_subs_2.c 
nmcDemo_SRCS += nmc_user_subs_1.c
nmcDemo_SRCS += nmc_user_subs_2.c 
nmcDemo_SRCS += nmc_demo.c


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

