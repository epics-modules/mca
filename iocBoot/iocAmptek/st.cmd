< envPaths
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("../../dbd/mcaAmptekApp.dbd",0,0)
mcaAmptekApp_registerRecordDeviceDriver(pdbbase) 

drvAmptekConfigure(Amptek1, 0, "164.54.160.255", 16347)

#asynSetTraceMask(Amptek1, 0, 255)
#asynSetTraceIOMask(Amptek1, 0, 2)

dbLoadRecords("../../mcaApp/Db/mca.db","P=mcaTest:,M=mca1,NCHAN=1024,DTYP=asynMCA,INP=@asyn(Amptek1)")

dbLoadRecords("$(ASYN)/db/asynRecord.db","P=mcaTest:,R=serial1,PORT=serial1,ADDR=0,OMAX=256,IMAX=256")

< save_restore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=mcaTest:")

