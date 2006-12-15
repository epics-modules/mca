TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaSISSupport.dbd
DBD += mcaSIS3820Support.dbd
DBD += sis3820Support.dbd
DBD += mcaSISTest.dbd

#=============================
LIBRARY_IOC_vxWorks += mcaSIS

mcaSIS_SRCS += drvSIS3820.c
mcaSIS_SRCS += devScalerSIS3820.c
mcaSIS_SRCS += drvMcaSIS3820Asyn.c
mcaSIS_SRCS += drvSTR7201.c
mcaSIS_SRCS += devSTR7201.c
mcaSIS_SRCS += devScalerSTR7201.c
mcaSIS_SRCS += mcaSISRegister.c
mcaSIS_SRCS += mcaSIS3820Register.c
mcaSIS_SRCS += sis3820Register.c

#==================================
PROD_IOC_vxWorks += mcaSISTest

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaSISTest_SRCS += mcaSISTest_registerRecordDeviceDriver.cpp
mcaSISTest_SRCS_DEFAULT += mcaSISTestMain.cpp
mcaSISTest_SRCS_vxWorks += -nil-
mcaSISTest_LIBS += mcaSIS
mcaSISTest_LIBS += mca
mcaSISTest_LIBS += std
mcaSISTest_LIBS += calc sscan autosave
mcaSISTest_LIBS += asyn
#mcaSISTest_LIBS += seq pv
mcaSISTest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

