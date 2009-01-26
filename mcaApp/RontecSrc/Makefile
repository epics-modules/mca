TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================


# <name>.dbd will be created from <name>Include.dbd
DBD += mcaRontec.dbd
DBD += mcaRontecSupport.dbd

#=============================
LIBRARY_IOC += mcaRontec

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaRontec_SRCS += drvMcaRontec.c
mcaRontec_LIBS += busy
mcaRontec_LIBS += calc sscan autosave
mcaRontec_LIBS += asyn 
mcaRontec_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC += mcaRontecApp

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaRontecApp_SRCS_DEFAULT += mcaRontec_registerRecordDeviceDriver.cpp mcaRontecMain.cpp
mcaRontecApp_SRCS_vxWorks += mcaRontec_registerRecordDeviceDriver.cpp
mcaRontecApp_LIBS += mca
mcaRontecApp_LIBS += mcaRontec
mcaRontecApp_LIBS += busy
mcaRontecApp_LIBS += calc sscan autosave
mcaRontecApp_LIBS += asyn
mcaRontecApp_SYS_LIBS_cygwin32 += rpc
mcaRontecApp_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

