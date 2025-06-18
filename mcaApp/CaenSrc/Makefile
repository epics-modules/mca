TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================
PYDEVSUP ?= /etc/epics
PYTHON ?= python3
include $(PYDEVSUP)/configure/CONFIG_PY

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaCaenApp.dbd
mcaCaenApp_DBD += 
DBD += mcaCaenSupport.dbd

# I only have a Linux environment to test
LIBRARY_IOC += mcaCaen
PROD_IOC    += mcaCaenApp

# Include where CAENDigitizer.h is
USR_INCLUDES += -I/usr/include/

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
#mcaCaen_SRCS += NetFinder.cpp 

mcaCaen_SRCS += drvCaen.cpp

mcaCaen_LIBS += asyn 
mcaCaen_LIBS += $(EPICS_BASE_IOC_LIBS)
#mcaCaen_SYS_LIBS += usb-1.0
#mcaCaen_LIBS_WIN32 += libusb-1.0

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaCaenApp_SRCS += mcaCaenApp_registerRecordDeviceDriver.cpp
mcaCaenApp_SRCS += DigitizerFunctions.cpp
mcaCaenApp_SRCS_DEFAULT += mcaCaenAppMain.cpp
mcaCaenApp_SRCS_vxWorks += -nil-
mcaCaenApp_LIBS += mcaCaen
mcaCaenApp_LIBS += mca
mcaCaenApp_LIBS += calc autosave
mcaCaenApp_LIBS += sscan busy
mcaCaenApp_LIBS += asyn
mcaCaenApp_LIBS += seq pv
mcaCaenApp_LIBS += pyDevSup$(PY_LD_VER)
mcaCaenApp_LIBS += $(EPICS_BASE_IOC_LIBS)
mcaCaenApp_SYS_LIBS += CAENDigitizer
#mcaCaenApp_SYS_LIBS += usb-1.0
#mcaCaenApp_LIBS_WIN32 += libusb-1.0

mcaCaenApp_DBD += base.dbd
mcaCaenApp_DBD += mcaCaenSupport.dbd
mcaCaenApp_DBD += mcaSupport.dbd
mcaCaenApp_DBD += asyn.dbd
mcaCaenApp_DBD += drvAsynIPPort.dbd
mcaCaenApp_DBD += calcSupport.dbd
mcaCaenApp_DBD += asSupport.dbd
mcaCaenApp_DBD += busySupport.dbd
mcaCaenApp_DBD += sscanSupport.dbd
mcaCaenApp_DBD += pyDevSup.dbd
mcaCaenApp_DBD += system.dbd

DIAGSTD ?= $(EPICS_BASE)
include $(DIAGSTD)/configure/fribdiagstdMakefile
include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
