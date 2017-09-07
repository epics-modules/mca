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

# Install USB library for Windows
# This came from https://sourceforge.net/projects/libusb/files/libusb-1.0/libusb-1.0.21/libusb-1.0.21.7z/download
INC_WIN32 += libusb.h
ifeq (win32-x86, $(findstring win32-x86, $(T_A)))
LIB_INSTALLS_WIN32    += ../os/win32-x86/libusb-1.0.lib 
BIN_INSTALLS_WIN32    += ../os/win32-x86/libusb-1.0.dll
else ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
LIB_INSTALLS_WIN32    += ../os/windows-x64/libusb-1.0.lib 
BIN_INSTALLS_WIN32    += ../os/windows-x64/libusb-1.0.dll
endif

ifeq ($(SHARED_LIBRARIES),YES)
  USR_CPPFLAGS_WIN32 += -DDLL_AMPTEK
  mcaAmptek.dll: USR_CPPFLAGS_WIN32 += -DDLL_EXPORT
endif

# Amptek's code uses WIN32 rather than _WIN32 
USR_CXXFLAGS_WIN32 += -DWIN32

LIBRARY_IOC_WIN32 += mcaAmptek
PROD_IOC_WIN32    += mcaAmptekApp
PROD_IOC_WIN32    += gccDppConsoleInet

# We only build this on Linux if LINUX_LIBUSB-1.0_INSTALLED is YES
ifeq ($(LINUX_LIBUSB-1.0_INSTALLED),YES)
  LIBRARY_IOC_Linux += mcaAmptek
  PROD_IOC_Linux    += mcaAmptekApp
  PROD_IOC_Linux    += gccDppConsoleInet
endif

USR_INCLUDES_Linux += -I/usr/include/libusb-1.0

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaAmptek_SRCS += AsciiCmdUtilities.cpp 
mcaAmptek_SRCS += ConsoleHelper.cpp 
mcaAmptek_SRCS += DP5Protocol.cpp 
mcaAmptek_SRCS += DP5Status.cpp 
mcaAmptek_SRCS += DppSocket.cpp 
mcaAmptek_SRCS += DppLibUsb.cpp 
mcaAmptek_SRCS += stringex.cpp 
mcaAmptek_SRCS += DppUtilities.cpp 
mcaAmptek_SRCS += ParsePacket.cpp 
mcaAmptek_SRCS += SendCommand.cpp 
mcaAmptek_SRCS += NetFinder.cpp 

mcaAmptek_SRCS += drvAmptek.cpp

mcaAmptek_LIBS += asyn 
mcaAmptek_LIBS += $(EPICS_BASE_IOC_LIBS)
mcaAmptek_SYS_LIBS_Linux += usb-1.0
mcaAmptek_LIBS_WIN32 += libusb-1.0

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
mcaAmptekApp_SYS_LIBS_Linux += usb-1.0
mcaAmptekApp_LIBS_WIN32 += libusb-1.0

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
gccDppConsoleInet_SYS_LIBS_Linux += usb-1.0
gccDppConsoleInet_LIBS_WIN32 += libusb-1.0

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
