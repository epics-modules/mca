TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

INC += DLL_DPP_Callback.h

ifeq (windows-x64, $(findstring windows-x64, $(T_A)))
LIB_INSTALLS  += ../os/WIN32/x64/XGL_DPP.lib
BIN_INSTALLS  += ../os/WIN32/x64/XGL_DPP.dll

else ifeq (linux-x86_64, $(findstring linux-x86_64, $(T_A)))
LIB_INSTALLS  += ../os/Linux/libXGL_DPP.so.1
LIB_INSTALLS  += ../os/Linux/libXGL_DPP.so
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
