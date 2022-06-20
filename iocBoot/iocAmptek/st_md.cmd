# For multiple-detector configurations

< envPaths

epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("$(MCA)/dbd/mcaAmptekApp.dbd",0,0)
mcaAmptekApp_registerRecordDeviceDriver(pdbbase) 

# Use this line for Ethernet
#drvAmptekConfigure(Amptek1, 0, "164.54.160.201")
#drvAmptekConfigure(Amptek2, 0, "164.54.160.202")
#drvAmptekConfigure(Amptek3, 0, "164.54.160.203")
# Use this line for USB
drvAmptekConfigure(Amptek1, 1, "1001")
drvAmptekConfigure(Amptek2, 1, "1005")
drvAmptekConfigure(Amptek3, 1, "1013")

#asynSetTraceMask(Amptek1, 0, 9)
asynSetTraceIOMask(Amptek1, 0, 2)

dbLoadRecords("$(MCA)/db/mca.db","P=mcaTest:,M=mca1,NCHAN=8192,DTYP=asynMCA,INP=@asyn(Amptek1)")
dbLoadRecords("$(MCA)/db/mca.db","P=mcaTest:,M=mca2,NCHAN=8192,DTYP=asynMCA,INP=@asyn(Amptek2)")
dbLoadRecords("$(MCA)/db/mca.db","P=mcaTest:,M=mca3,NCHAN=8192,DTYP=asynMCA,INP=@asyn(Amptek3)")
dbLoadRecords("$(MCA)/db/3element.db","P=mcaTest:,BASENAME=mca")
dbLoadRecords("$(MCA)/db/Amptek.db","P=mcaTest:,R=Amptek1:,PORT=Amptek1")
dbLoadRecords("$(MCA)/db/Amptek.db","P=mcaTest:,R=Amptek2:,PORT=Amptek2")
dbLoadRecords("$(MCA)/db/Amptek.db","P=mcaTest:,R=Amptek3:,PORT=Amptek3")
dbLoadTemplate("Amptek_SCAs.substitutions")

dbLoadRecords("$(ASYN)/db/asynRecord.db","P=mcaTest:,R=asyn1,PORT=Amptek1,ADDR=0,OMAX=256,IMAX=256")
dbLoadRecords("$(ASYN)/db/asynRecord.db","P=mcaTest:,R=asyn2,PORT=Amptek2,ADDR=0,OMAX=256,IMAX=256")
dbLoadRecords("$(ASYN)/db/asynRecord.db","P=mcaTest:,R=asyn3,PORT=Amptek3,ADDR=0,OMAX=256,IMAX=256")

< ../save_restore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=mcaTest:,M=mca1,R=Amptek1:")
create_monitor_set("auto_settings.req",30,"P=mcaTest:,M=mca2,R=Amptek2:")
create_monitor_set("auto_settings.req",30,"P=mcaTest:,M=mca3,R=Amptek3:")

#asynReport 1 Amptek1
#asynReport 1 Amptek2
#asynReport 1 Amptek3
