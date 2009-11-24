TOP = ..
include $(TOP)/configure/CONFIG

# Always built mcaSrc, device-independent
DIRS += mcaSrc

# Support libraries for Canberra under Cygwin
DIRS += CanberraSupport

# Support for Canberra Ethernet acquisition devices
DIRS += CanberraSrc
CanberraSrc_DEPEND_DIRS = CanberraSupport mcaSrc

# Support for SIS and Struck VME multiscalers
DIRS += SISSrc
SISSrc_DEPEND_DIRS = mcaSrc

# Support for Rontec XFlash
DIRS += RontecSrc
RontecSrc_DEPEND_DIRS = mcaSrc

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

