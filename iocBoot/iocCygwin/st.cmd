< envPaths
epicsEnvSet(STARTUP,$(TOP)/iocBoot/$(IOC))

dbLoadDatabase("../../dbd/mcaCanberra.dbd",0,0)
mcaCanberra_registerRecordDeviceDriver(pdbbase) 

#var aimDebug 10

# AIMConfig(portName, ethernet_address, portNumber(1 or 2), maxChans,
#           maxSignals, maxSequences, ethernetDevice)
# On Windows the ethernetDevice name is unique to each network card.  
# You can get this number for your Window machine by
# using the "regedit" utility, and doing an "export" of the key:
# [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\NetworkCards]
# This should look like:
# [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\NetworkCards]
#
# [HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\NetworkCards\2]
# "ServiceName"="{15B576D2-6DF4-4C9F-B53C-DBF76B53194E}"
# "Description"="3Com 3C920 Integrated Fast Ethernet Controller (3C905C-TX Compatible)"
# The number that is needed in the ServiceName field.
# Copy this number and paste into the AIMConfig command below
AIMConfig("AIM1/1", 0x59e, 1, 2048, 1, 1, "\Device\NPF_{E9C3D739-42E6-4239-9E45-0763306D1802}")
AIMConfig("AIM1/2", 0x59e, 2, 2048, 1, 1, "\Device\NPF_{E9C3D739-42E6-4239-9E45-0763306D1802}")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=mcaTest:,M=aim_adc1,DTYP=asynMCA,INP=@asyn(AIM1/1 0),NCHAN=2048")
dbLoadRecords("$(MCA)/mcaApp/Db/mca.db", "P=mcaTest:,M=aim_adc2,DTYP=asynMCA,INP=@asyn(AIM1/2 0),NCHAN=2048")

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
icbConfig("icbAdc1", 0x59e, 5, 0)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_adc.db", "P=mcaTest:,ADC=adc1,PORT=icbAdc1")
icbConfig("icbAmp1", 0x59e, 3, 1)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_amp.db", "P=mcaTest:,AMP=amp1,PORT=icbAmp1")
icbConfig("icbHvps1", 0x59e, 2, 2)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_hvps.db", "P=mcaTest:,HVPS=hvps1,PORT=icbHvps1,LIMIT=1000")
icbConfig("icbTca1", 0x59e, 8, 3)
dbLoadRecords("$(MCA)/mcaApp/Db/icb_tca.db", "P=mcaTest:,TCA=tca1,MCA=aim_adc2,PORT=icbTca1")
#icbConfig("icbDsp1", 0x8058, 0, 4)
#dbLoadRecords("$(MCA)/mcaApp/Db/icbDsp.db", "P=mcaTest:,DSP=dsp1,PORT=icbDsp1")

mcaAIMShowModules

#asynSetTraceMask "AIM1/1",0,0xff
#asynSetTraceMask "icbTca1",0,0x13
#asynSetTraceMask "icbHvps1",0,0xff

< save_restore.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings.req",30,"P=mcaTest:")

