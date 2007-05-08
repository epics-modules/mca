TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaSISSupport.dbd
DBD += mcaSIS3820Support.dbd
DBD += mcaSISTest.dbd

#=============================
LIBRARY_IOC += mcaSIS

mcaSIS_SRCS += drvMcaSIS3820Asyn.c
mcaSIS_SRCS += mcaSIS3820Register.c
mcaSIS_SRCS_vxWorks += drvSTR7201.c
mcaSIS_SRCS_vxWorks += devSTR7201.c
mcaSIS_SRCS_vxWorks += devScalerSTR7201.c
mcaSIS_SRCS_vxWorks += mcaSISRegister.c

#==================================
# If your version of Linux has support for VME devLib uncomment following line
#PROD_IOC_Linux += mcaSISTest
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
mcaSISTest_LIBS += seq pv
mcaSISTest_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

