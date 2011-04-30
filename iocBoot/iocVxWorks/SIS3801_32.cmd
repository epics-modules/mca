# Example vxWorks startup file for SIS3801

< ../nfsCommands
< cdCommands

cd topbin
ld < SIS38XXTest.munch

errlogInit(20000)

cd startup

dbLoadDatabase("../../dbd/SIS38XXTest.dbd",0,0)
SIS38XXTest_registerRecordDeviceDriver(pdbbase) 

#drvSIS3801Config("Port name",
#                  baseAddress,
#                  interruptVector, 
#                  int interruptLevel,
#                  channels,
#                  signals)
drvSIS3801Config("SIS3801/1", 0x90000000, 220, 6, 10000, 32)
dbLoadTemplate("SIS3801_32.substitutions")

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "50000")
epicsPrtEnvParams

asynSetTraceIOMask("SIS3801/1",0,2)
asynSetTraceFile("SIS3801/1",0,"SIS3801.out")
asynSetTraceMask("SIS3801/1",0,0xff)

< save_restore_SIS38XX.cmd

iocInit()

seq(&SIS38XX_SNL, "P=SIS:3801:, NUM_MCAS=32, MCA=mca")

# save settings every thirty seconds
create_monitor_set("auto_settings_SIS38XX.req",30,"P=SIS:3801")

