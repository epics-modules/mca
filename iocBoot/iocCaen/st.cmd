#!../../bin/linux-x86_64/mcaCaenApp

< envPaths

epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("$(MCA)/dbd/mcaCaenApp.dbd",0,0)
mcaCaenApp_registerRecordDeviceDriver(pdbbase) 

# See CAENDigitizer documentation on OpenDigitizer for more info on this function
# drvCaenConfigure(Portname, LinkType, LinkNum, ConetNode, VMEBaseAddress)
drvCaenConfigure(Caen1, 5, 12345, 0, 0)

#asynSetTraceMask(Caen1, 7, 15)
#asynSetTraceIOMask(Caen1, 7, 15)
epicsEnvSet("P", "CAEN_MCA:")
epicsEnvSet("PORT", "Caen1")

# Board wide parameters
dbLoadRecords("$(MCA)/db/Caen.db","P=$(P),R=MCA01:,PORT=$(PORT)")
# Multi-ADC auxilary PVs
dbLoadRecords("$(MCA)/db/CaenBoard.db","P=$(P),M=MCA01:,PORT=$(PORT),M1=MCA01_CH01,M2=MCA01_CH02,M3=MCA01_CH03,M4=MCA01_CH04,M5=MCA01_CH05,M6=MCA01_CH06,M7=MCA01_CH07,M8=MCA01_CH08")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH01,PORT=$(PORT),CHAN=0,NCHAN=16384,DTYP=asynMCA")
# MCA records and per-ADC parameters
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH02,PORT=$(PORT),CHAN=1,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH03,PORT=$(PORT),CHAN=2,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH04,PORT=$(PORT),CHAN=3,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH05,PORT=$(PORT),CHAN=4,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH06,PORT=$(PORT),CHAN=5,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH07,PORT=$(PORT),CHAN=6,NCHAN=16384,DTYP=asynMCA")
dbLoadRecords("$(MCA)/db/CaenChannel.db","P=$(P),R=MCA01_CH08,PORT=$(PORT),CHAN=7,NCHAN=16384,DTYP=asynMCA")

iocInit()
