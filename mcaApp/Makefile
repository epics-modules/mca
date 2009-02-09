TOP = ..
include $(TOP)/configure/CONFIG

# Always built mcaSrc, device-independent
DIRS += mcaSrc

# Support for Canberra Ethernet acquisition devices
ifeq ($(EPICS_HOST_ARCH), cygwin-x86)
ifeq ($(CYGWIN_NET_INSTALLED), YES)
DIRS += CanberraSrc
CanberraSrc_DEPEND_DIRS = mcaSrc
endif
else
ifeq ($(EPICS_HOST_ARCH), linux-x86)
ifeq ($(LINUX_NET_INSTALLED), YES)
DIRS += CanberraSrc
CanberraSrc_DEPEND_DIRS = mcaSrc
endif
else
DIRS += CanberraSrc
CanberraSrc_DEPEND_DIRS = mcaSrc
endif
endif

# Support for SIS and Struck VME multiscalers
DIRS += SISSrc
SISSrc_DEPEND_DIRS = mcaSrc

# Support for Rontec XFlash
DIRS += RontecSrc
RontecSrc_DEPEND_DIRS = mcaSrc

DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *db*))
DIRS := $(DIRS) $(filter-out $(DIRS), $(wildcard *Db*))
include $(TOP)/configure/RULES_DIRS

