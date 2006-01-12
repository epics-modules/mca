TOP = ..
include $(TOP)/configure/CONFIG

# Always built mcaSrc, device-independent
DIRS += mcaSrc

# Support for Canberra Ethernet acquisition devices
DIRS += CanberraSrc

# Support for SIS and Struck VME multiscalers
DIRS += SISSrc

# Support for Rontec XFlash
DIRS += RontecSrc

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

