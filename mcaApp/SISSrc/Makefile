TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CPPFLAGS += -UUSE_DMA

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaSTR7201Support.dbd
DBD += mcaSIS3820Support.dbd
DBD += mcaSISTest.dbd

#=============================
# Build the library for ARCHs with VME capability.
# This is vxWorks, RTEMS and Linux (with PCI/VME bridge)
LIBRARY_IOC_vxWorks += mcaSIS
LIBRARY_IOC_RTEMS += mcaSIS
# If your version of Linux has support for VME devLib uncomment following line
#LIBRARY_IOC_Linux += mcaSIS

mcaSIS_SRCS += drvSIS38XX.cpp
mcaSIS_SRCS += drvSIS3820.cpp
mcaSIS_SRCS += SIS38XX_SNL.st
mcaSIS_SRCS += drvMcaSIS3820Asyn.c
mcaSIS_SRCS += mcaSIS3820Register.c
mcaSIS_SRCS += drvSTR7201.c
mcaSIS_SRCS += devSTR7201.c
mcaSIS_SRCS += devScalerSTR7201.c
mcaSIS_SRCS += mcaSTR7201Register.c
mcaSIS_LIBS += mca
mcaSIS_LIBS += std
mcaSIS_LIBS += busy
mcaSIS_LIBS += calc autosave
mcaSIS_LIBS += asyn
mcaSIS_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_vxWorks += mcaSISTest
PROD_IOC_RTEMS += mcaSISTest
# If your version of Linux has support for VME devLib uncomment following line
#PROD_IOC_Linux += mcaSISTest

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaSISTest_SRCS += mcaSISTest_registerRecordDeviceDriver.cpp
mcaSISTest_SRCS_DEFAULT += mcaSISTestMain.cpp
mcaSISTest_SRCS_vxWorks += -nil-
mcaSISTest_LIBS += mcaSIS
mcaSISTest_LIBS += mca
mcaSISTest_LIBS += std
mcaSISTest_LIBS += busy
mcaSISTest_LIBS += calc autosave
mcaSISTest_LIBS += sscan
mcaSISTest_LIBS += asyn
mcaSISTest_LIBS += seq pv
mcaSISTest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

