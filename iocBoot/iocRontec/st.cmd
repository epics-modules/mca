< envPaths
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("../../dbd/mcaRontec.dbd",0,0)
mcaRontec_registerRecordDeviceDriver(pdbbase) 

# COM 1 connected to Rontec at 38400 baud
#drvAsynSerialPortConfigure("portName","ttyName",priority,noAutoConnect,
#                            noProcessEos)
drvAsynSerialPortConfigure("serial1", "/dev/ttyS0", 0, 0, 0)
asynSetOption(serial1,0,baud,38400)
#asynOctetSetInputEos(const char *portName, int addr,
#                     const char *eosin,const char *drvInfo)
asynOctetSetInputEos("serial1",0,"\r")
# asynOctetSetOutputEos(const char *portName, int addr,
#                       const char *eosout,const char *drvInfo)
asynOctetSetOutputEos("serial1",0,"\r")
# Make port available from the iocsh command line
#asynOctetConnect(const char *entry, const char *port, int addr,
#                 int timeout, int buffer_len, const char *drvInfo)
asynOctetConnect("serial1", "serial1")

#asynSetTraceMask(serial1, 0, 3)
#asynSetTraceIOMask(serial1, 0, 4)

RontecConfig(Rontec1, serial1, 0)

#asynSetTraceMask(Rontec1, 0, 255)
#asynSetTraceIOMask(Rontec1, 0, 2)

dbLoadRecords("../../mcaApp/Db/mca.db","P=mcaTest:,M=mca1,NCHAN=1024,DTYP=asynMCA,INP=@asyn(Rontec1)")
dbLoadRecords("../../mcaApp/Db/RontecXFlash.db","P=mcaTest:,R=Rontec1,PORT=serial1")

dbLoadRecords("$(ASYN)/db/asynRecord.db","P=mcaTest:,R=serial1,PORT=serial1,ADDR=0,OMAX=256,IMAX=256")

< save_restore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=mcaTest:")

