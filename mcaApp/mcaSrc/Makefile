TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# xxxRecord.h will be created from xxxRecord.dbd
DBDINC += mcaRecord

# <name>.dbd will be created from <name>Include.dbd
DBD += mca.dbd
DBD += mcaSupport.dbd

#=============================
LIBRARY_IOC_DEFAULT += mca
LIBRARY_IOC_solaris += -nil-

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mca_SRCS += mcaRecord.c
mca_SRCS += devMCA_soft.c
mca_SRCS += devMcaAsyn.c
mca_SRCS += drvFastSweep.c
mca_LIBS += sscan
mca_LIBS += asyn
mca_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += mca.h
INC += drvMca.h
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

