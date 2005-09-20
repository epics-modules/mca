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
mcaRontec_LIBS += calc sscan autosave
mcaRontec_LIBS += asyn 
mcaRontec_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC += mcaRontecApp

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaRontecApp_SRCS += mcaRontec_registerRecordDeviceDriver.cpp
mcaRontecApp_SRCS_DEFAULT += mcaRontecMain.cpp
mcaRontecApp_SRCS_vxWorks += -nil-
mcaRontecApp_LIBS += mca
mcaRontecApp_LIBS += mcaRontec
mcaRontecApp_LIBS += calc sscan autosave
mcaRontecApp_LIBS += asyn
mcaRontecApp_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

