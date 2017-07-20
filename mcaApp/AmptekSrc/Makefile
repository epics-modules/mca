TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaAmptekApp.dbd
DBD += mcaAmptekSupport.dbd

# Linux
LIBRARY_IOC_Linux += mcaAmptek
PROD_IOC_Linux    += dppConsoleInet
#PROD_IOC_Linux    += mcaAmptekApp

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAmptek_SRCS += AsciiCmdUtilities.cpp 
mcaAmptek_SRCS += ConsoleHelper.cpp 
mcaAmptek_SRCS += DP5Protocol.cpp 
mcaAmptek_SRCS += DP5Status.cpp 
mcaAmptek_SRCS += DppSocket.cpp 
mcaAmptek_SRCS += DppUtilities.cpp 
mcaAmptek_SRCS += MSecTimer.cpp 
mcaAmptek_SRCS += NetFinder.cpp 
mcaAmptek_SRCS += ParsePacket.cpp 
mcaAmptek_SRCS += SendCommand.cpp 

mcaAmptek_LIBS += asyn 
mcaAmptek_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAmptekApp_SRCS  += mcaAmptek_registerRecordDeviceDriver.cpp
mcaAmptek_SRCS_DEFAULT += mcaAmptekMain.cpp
mcaAmptek_SRCS_vxWorks += -nil-
mcaAmptek_LIBS        += mca
mcaAmptek_LIBS        += mcaAmptek
mcaAmptek_LIBS        += calc autosave
mcaAmptek_LIBS        += sscan busy
mcaAmptek_LIBS        += asyn
mcaAmptek_LIBS        += seq pv
mcaAmptek_LIBS        += $(EPICS_BASE_IOC_LIBS)

#=============================
dppConsoleInet_SRCS += dppConsoleInet.cpp 
dppConsoleInet_LIBS += mcaAmptek


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
