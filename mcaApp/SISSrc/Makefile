TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaSISSupport.dbd

#=============================
LIBRARY_IOC_vxWorks += mcaSIS

mcaSIS_SRCS += drvSTR7201.c
mcaSIS_SRCS += devSTR7201.c
mcaSIS_SRCS += devScalerSTR7201.c
mcaSIS_SRCS += mcaSISRegister.c

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

