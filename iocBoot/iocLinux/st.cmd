# Simple startup script
# error log to console
# eltc 1

#setRMRClientDebug 10
#setRMRServerDebug 10
#
dbLoadDatabase("../../dbd/mcaCanberra.dbd",0,0)
registerRecordDeviceDriver(pdbbase) 
dbLoadRecords("../../mcaApp/Db/mca.db","P=mcaTest:,M=aim_adc1,NCHAN=2048,NUSE=2048,DTYPE=MPF MCA,INP=#C0 S0@AIMServ")
dbLoadRecords("../../mcaApp/Db/icb_adc.db", "P=mcaTest:,ADC=adc1,CARD=0,SERVER=icb/1,ADDR=0")
dbLoadRecords("../../mcaApp/Db/icb_tca.db", "P=mcaTest:,TCA=tca1,MCA=aim_adc1,CARD=0,SERVER=icbTca/1,ADDR=0")
#dbLoadRecords("../../../std/stdApp/Db/scan.db","P=13LAB:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")
dbLoadRecords("../../../std/stdApp/Db/pid_control.db","P=mcaTest:,PID=PID1,INP=test,OUT=test,SCAN=0,KP=1.,KI=1.,KD=0.,LOPR=0,DRVL=0,HOPR=100.,DRVH=100.,PREC=3")

#
routerInit
localMessageRouterStart(0)

#setMcaDebug 10 10 10 10
#setTcaDebug(10)
#setIcbDebug(10)

#AIMConfig(serverName, ethernetAddress, ADC port, maxChans,
#          maxSignals, maxSequences, ethernetDevice, queueSize)
AIMConfig("AIMServ", 0x59e, 1, 2048, 1, 1, "eth1", 1000)
mcaAIMShowModules()

#icbConfig(serverName, maxModules, address, queueSize)
icbConfig("icb/1", 10, "NI59E:5", 100)

#icbTcaConfig(serverName, maxModules, address, queueSize)
icbTcaConfig("icbTca/1", 1, "NI59E:8", 100)

iocInit()
