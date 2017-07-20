TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
#DBD += mcaAmptekApp.dbd
#DBD += mcaAmptekSupport.dbd

# Linux
LIBRARY_IOC += mcaAmptek
PROD_IOC    += dppConsoleInet
#PROD_IOC    += mcaAmptekApp

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAmptek_SRCS += AsciiCmdUtilities.cpp 
mcaAmptek_SRCS += ConsoleHelper.cpp 
mcaAmptek_SRCS += DP5Protocol.cpp 
mcaAmptek_SRCS += DP5Status.cpp 
mcaAmptek_SRCS += DppSocket.cpp 
mcaAmptek_SRCS += stringex.cpp 
mcaAmptek_SRCS += DppUtilities.cpp 
mcaAmptek_SRCS += ParsePacket.cpp 
mcaAmptek_SRCS += SendCommand.cpp 

mcaAmptek_LIBS += asyn 
mcaAmptek_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
#mcaAmptekApp_SRCS  += mcaAmptek_registerRecordDeviceDriver.cpp
#mcaAmptekApp_SRCS_DEFAULT += mcaAmptekMain.cpp
#mcaAmptekApp_SRCS_vxWorks += -nil-
mcaAmptekApp_LIBS        += mca
mcaAmptekApp_LIBS        += mcaAmptek
mcaAmptekApp_LIBS        += calc autosave
mcaAmptekApp_LIBS        += sscan busy
mcaAmptekApp_LIBS        += asyn
mcaAmptekApp_LIBS        += seq pv
mcaAmptekApp_LIBS        += $(EPICS_BASE_IOC_LIBS)

#=============================
dppConsoleInet_SRCS += dppConsoleInet.cpp 
dppConsoleInet_LIBS += mcaAmptek
dppConsoleInet_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
