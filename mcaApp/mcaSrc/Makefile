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
LIBRARY_IOC += mca

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mca_SRCS += mcaRecord.c
mca_SRCS += devMCA_soft.c
mca_SRCS += devMcaMpf.cc
mca_SRCS += fastSweep.cc
mca_SRCS += mcaRegister.c
mca_LIBS += mpf mpfEpics std
mca_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += fastSweep.h
INC += mca.h
INC += DevMcaMpf.h
INC += mcamsgdef.h
#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

