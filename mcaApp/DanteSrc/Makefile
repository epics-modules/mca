TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================

# <name>.dbd will be created from <name>Include.dbd
DBD += mcaDante.dbd
DBD += mcaDanteSupport.dbd

USR_CXXFLAGS_Linux += -std=c++11
#LIBRARY_IOC_Linux += mcaDante
#PROD_IOC_Linux    += mcaDanteApp

# WIN32 uses WinPcap
#LIBRARY_IOC_WIN32   += mcaDante
#PROD_IOC_WIN32      += mcaDanteApp
LIB_SYS_LIBS_WIN32  += ws2_32
PROD_SYS_LIBS_WIN32 += ws2_32

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
#mcaDante_SRCS += dante.cpp

mcaDante_LIBS += asyn 
mcaDante_LIBS += $(EPICS_BASE_IOC_LIBS)

## <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
mcaDanteApp_SRCS   += mcaDante_registerRecordDeviceDriver.cpp
mcaDanteApp_SRCS   += mcaDanteMain.cpp
mcaDanteApp_LIBS   += mca
mcaDanteApp_LIBS   += mcaDante
mcaDanteApp_LIBS   += XGL_DPP
mcaDanteApp_LIBS   += calc autosave
mcaDanteApp_LIBS   += sscan busy
mcaDanteApp_LIBS   += asyn
mcaDanteApp_LIBS   += seq pv
mcaDanteApp_LIBS   += $(EPICS_BASE_IOC_LIBS)


#=============================
PROD_IOC_Linux += danteTest1
PROD_IOC_WIN32 += danteTest1
danteTest1_SRC   += danteTest1.cpp
danteTest1_LIBS  += XGL_DPP

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
