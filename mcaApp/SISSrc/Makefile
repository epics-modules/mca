TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#USR_CPPFLAGS += -UUSE_DMA

# <name>.dbd will be created from <name>Include.dbd
DBD += SIS38XXSupport.dbd
DBD += SIS38XXTest.dbd

#=============================
# Build the library for ARCHs with VME capability.
# This is vxWorks, RTEMS and Linux (with PCI/VME bridge)
LIBRARY_IOC_vxWorks += SIS38XX
LIBRARY_IOC_RTEMS += SIS38XX
# If your version of Linux has support for VME devLib uncomment following line
#LIBRARY_IOC_Linux += SIS38XX

SIS38XX_SRCS += drvSIS38XX.cpp
SIS38XX_SRCS += drvSIS3820.cpp
SIS38XX_SRCS += drvSIS3801.cpp
SIS38XX_SRCS += SIS38XX_SNL.st
SIS38XX_LIBS += mca
SIS38XX_LIBS += std
SIS38XX_LIBS += busy
SIS38XX_LIBS += calc autosave
SIS38XX_LIBS += asyn
SIS38XX_LIBS += $(EPICS_BASE_IOC_LIBS)

#==================================
PROD_IOC_vxWorks += SIS38XXTest
PROD_IOC_RTEMS += SIS38XXTest
# If your version of Linux has support for VME devLib uncomment following line
#PROD_IOC_Linux += SIS38XXTest

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
SIS38XXTest_SRCS += SIS38XXTest_registerRecordDeviceDriver.cpp
SIS38XXTest_SRCS_DEFAULT += SIS38XXTestMain.cpp
SIS38XXTest_SRCS_vxWorks += -nil-
SIS38XXTest_LIBS += SIS38XX
SIS38XXTest_LIBS += mca
SIS38XXTest_LIBS += std
SIS38XXTest_LIBS += busy
SIS38XXTest_LIBS += calc autosave
SIS38XXTest_LIBS += sscan
SIS38XXTest_LIBS += asyn
SIS38XXTest_LIBS += seq pv
SIS38XXTest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

