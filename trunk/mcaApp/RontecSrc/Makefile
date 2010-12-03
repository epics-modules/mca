TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================


# <name>.dbd will be created from <name>Include.dbd
DBD += mcaRontec.dbd
DBD += mcaRontecSupport.dbd

DBD += mcaBrukerQM100.dbd
DBD += mcaBrukerQM100Support.dbd

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
mcaRontecApp_SYS_LIBS_cygwin32 += $(CYGWIN_RPC_LIB)
mcaRontecApp_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================
LIBRARY_IOC_WIN32 += mcaBrukerQM100

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaBrukerQM100_SRCS += drvMcaBrukerQM100.c
mcaBrukerQM100_LIBS += busy calc sscan autosave
mcaBrukerQM100_LIBS += asyn 
mcaBrukerQM100_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_WIN32 += mcaBrukerQM100App

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaBrukerQM100App_SRCS_WIN32 += mcaBrukerQM100_registerRecordDeviceDriver.cpp mcaBrukerQM100Main.cpp
mcaBrukerQM100App_LIBS += mca
mcaBrukerQM100App_LIBS += mcaBrukerQM100
mcaBrukerQM100App_LIBS += busy calc sscan autosave
mcaBrukerQM100App_LIBS += asyn
mcaBrukerQM100App_LIBS += $(EPICS_BASE_IOC_LIBS)


# uncomment, if using Bruker QM100-Server
#BIN_INSTALLS_WIN32 += ../RTIfcClient.dll

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

