TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# libnet needs this
USR_CFLAGS_linux-x86 += -g `libnet-config --defines` 
USR_CXXFLAGS_linux-x86 += -g `libnet-config --defines`
USR_CFLAGS_darwin-ppc += -g `libnet-config --defines` 
USR_CXXFLAGS_darwin-ppc += -g `libnet-config --defines`

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaCanberra.dbd
DBD += mcaCanberraSupport.dbd

#=============================
# Right now we only build this on vxWorks and Linux
LIBRARY_IOC_Linux += mcaCanberra
LIBRARY_IOC_vxWorks += mcaCanberra

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
mcaCanberra_LIBS += calc sscan autosave
mcaCanberra_LIBS += asyn 
mcaCanberra_LIBS += $(EPICS_BASE_IOC_LIBS)
# should be mcaAIM_LIBS_linux-x86 which won't work
mcaCanberra_SYS_LIBS_DEFAULT += net pcap
mcaCanberra_SYS_LIBS_vxWorks += -nil-

#==================================
PROD_IOC_vxWorks += mcaAIM
PROD_IOC_Linux += mcaAIM
## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAIM_SRCS += mcaCanberra_registerRecordDeviceDriver.cpp
mcaAIM_SRCS_DEFAULT += mcaCanberraMain.cpp
mcaAIM_SRCS_vxWorks += -nil-
mcaAIM_LIBS += mca
mcaAIM_LIBS += mcaCanberra
mcaAIM_LIBS += calc sscan autosave
mcaAIM_LIBS += asyn
mcaAIM_LIBS += $(EPICS_BASE_IOC_LIBS)

# should be mcaAIM_LIBS_linux-x86 which won't work
mcaAIM_SYS_LIBS_DEFAULT += net pcap
mcaAIM_SYS_LIBS_vxWorks += -nil-

mcaAIM_LDOBJS_DEFAULT += mcaCanberraRegister.o
mcaAIM_LDOBJS_vxWorks += -nil-

#=============================
PROD_IOC_vxWorks += nmcTest
PROD_IOC_Linux += nmcTest

nmcTest_LIBS += ca Com 
nmcTest_SYS_LIBS_DEFAULT += net pcap
nmcTest_SYS_LIBS_vxWorks += -nil-

nmcTest_SRCS += nmc_comm_subs_1.c 
nmcTest_SRCS += nmc_comm_subs_2.c 
nmcTest_SRCS += nmc_user_subs_1.c
nmcTest_SRCS += nmc_user_subs_2.c 
nmcTest_SRCS += nmc_test.c

#=============================
PROD_IOC_vxWorks += nmcDemo
PROD_IOC_Linux += nmcDemo

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

