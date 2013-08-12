< envPaths
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("../../dbd/mcaCanberra.dbd",0,0)
mcaCanberra_registerRecordDeviceDriver(pdbbase) 

#var aimDebug 10

# AIMConfig(portName, ethernet_address, portNumber(1 or 2), maxChans,
#           maxSignals, maxSequences, ethernetDevice)
AIMConfig("AIM1/1", 0x8058, 1, 2048, 1, 1, "eth1")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=mcaTest:,M=aim_adc1,DTYP=asynMCA,INP=@asyn(AIM1/1 0),NCHAN=2048")

#icbConfig(portName, module, ethernetAddress, icbAddress, moduleType)
#   portName to give to this asyn port
#   ethernetAddress - Ethernet address of module, low order 16 bits
#   icbAddress - rotary switch setting inside ICB module
#   moduleType
#      0 = ADC
#      1 = Amplifier
#      2 = HVPS
#      3 = TCA
#      4 = DSP
icbConfig("icbDsp1", 0x8058, 0, 4)
dbLoadRecords("$(MCA)/mcaApp/Db/icbDsp.db", "P=mcaTest:,DSP=dsp1,PORT=icbDsp1")

DSA2000Config("DSA2000",0x8058)
dbLoadRecords("$(MCA)/mcaApp/Db/DSA2000_HVPS.db", "P=mcaTest:,HVPS=hvps1:,PORT=DSA2000,LIMIT=5000")


#asynSetTraceMask "AIM1/1",0,0xff
#asynSetTraceMask "icbTca1",0,0x13
#asynSetTraceMask "icbHvps1",0,0xff
#asynSetTraceMask "DSA2000",0,0xff

< save_restore.cmd

set_pass0_restoreFile("DSA2000_settings.sav")
set_pass1_restoreFile("DSA2000_settings.sav")

iocInit()

# save settings every thirty seconds
create_monitor_set("DSA2000_settings.req",30,"P=mcaTest:")

