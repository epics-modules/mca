TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaAmptekApp.dbd
mcaAmptekApp_DBD += 
DBD += mcaAmptekSupport.dbd

# Amptek's code uses WIN32 rather than _WIN32 
USR_CXXFLAGS_WIN32 += -DWIN32

# Linux
LIBRARY_IOC += mcaAmptek
PROD_IOC    += gccDppConsoleInet
PROD_IOC    += mcaAmptekApp

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
mcaAmptek_SRCS += NetFinder.cpp 

mcaAmptek_SRCS += drvAmptek.cpp

mcaAmptek_LIBS += asyn 
mcaAmptek_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAmptekApp_SRCS += mcaAmptekApp_registerRecordDeviceDriver.cpp
mcaAmptekApp_SRCS_DEFAULT += mcaAmptekAppMain.cpp
mcaAmptekApp_SRCS_vxWorks += -nil-
mcaAmptekApp_LIBS += mcaAmptek
mcaAmptekApp_LIBS += mca
mcaAmptekApp_LIBS += calc autosave
mcaAmptekApp_LIBS += sscan busy
mcaAmptekApp_LIBS += asyn
mcaAmptekApp_LIBS += seq pv
mcaAmptekApp_LIBS += $(EPICS_BASE_IOC_LIBS)

mcaAmptekApp_DBD += base.dbd
mcaAmptekApp_DBD += mcaAmptekSupport.dbd
mcaAmptekApp_DBD += mcaSupport.dbd
mcaAmptekApp_DBD += asyn.dbd
mcaAmptekApp_DBD += drvAsynSerialPort.dbd
mcaAmptekApp_DBD += drvAsynIPPort.dbd
mcaAmptekApp_DBD += calcSupport.dbd
mcaAmptekApp_DBD += asSupport.dbd
mcaAmptekApp_DBD += busySupport.dbd
mcaAmptekApp_DBD += sscanSupport.dbd


#=============================
gccDppConsoleInet_SRCS += gccDppConsoleInet.cpp 
gccDppConsoleInet_LIBS += mcaAmptek
gccDppConsoleInet_LIBS += $(EPICS_BASE_IOC_LIBS)


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
