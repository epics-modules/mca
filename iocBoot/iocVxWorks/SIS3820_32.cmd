# Example vxWorks startup file for SIS3820

< ../nfsCommands
< cdCommands

cd topbin
ld < SIS38XXTest.munch

errlogInit(20000)

cd startup

dbLoadDatabase("../../dbd/mcaSISTest.dbd",0,0)
SIS38XXTest_registerRecordDeviceDriver(pdbbase) 

#drvSIS3820Config("Port name",
#                  baseAddress,
#                  interruptVector, 
#                  int interruptLevel,
#                  channels,
#                  signals,
#                  use DMA
#                  fifoBufferWords)
drvSIS3820Config("SIS3820/1", 0xA8000000, 224, 6, 100000, 32, 1, 0x200000)
dbLoadTemplate("SIS3820_2.substitutions")

epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", "10000000")
epicsPrtEnvParams

asynSetTraceIOMask("SIS3820/1",0,2)
#asynSetTraceFile("SIS3820/1",0,"SIS3820.out")
#asynSetTraceMask("SIS3820/1",0,0xff)

< save_restore_SIS38XX.cmd

iocInit()

seq(&SIS38XX_SNL, "P=SIS:3820:, NUM_MCAS=2, MCA=mca")

# save settings every thirty seconds
create_monitor_set("auto_settings_SIS38XX.req",30,"P=SIS:3820:")

