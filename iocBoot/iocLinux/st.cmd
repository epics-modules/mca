# Simple startup script
# error log to console
# eltc 1

#setRMRClientDebug 10
#setRMRServerDebug 10
#
dbLoadDatabase("../../dbd/mcaCanberra.dbd",0,0)
registerRecordDeviceDriver(pdbbase) 
#dbLoadRecords("../../../std/stdApp/Db/scan.db","P=mcaTest:,MAXPTS1=2000,MAXPTS2=200,MAXPTS3=20,MAXPTS4=10,MAXPTSH=10")
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
dbLoadRecords("../../mcaApp/Db/mca.db","P=mcaTest:,M=aim_adc1,NCHAN=2048,NUSE=2048,DTYPE=MPF MCA,INP=#C0 S0@AIMServ")

#icbSetup(serverName, maxModules, queueSize)
icbSetup("icb/1", 10, 100)
#icbConfig(serverName, module, ethernetAddress, icbAddress)
icbConfig("icb/1", 0, 0x59e, 5)
dbLoadRecords("../../../mca/mcaApp/Db/icb_adc.db", "P=13LAB:,ADC=adc1,CARD=0,SERVER=icb/1,ADDR=0")
icbConfig("icb/1", 1, 0x59e, 3)
dbLoadRecords("../../../mca/mcaApp/Db/icb_amp.db", "P=13LAB:,AMP=amp1,CARD=0,SERVER=icb/1,ADDR=1")
icbConfig("icb/1", 2, 0x59e, 2)
dbLoadRecords("../../../mca/mcaApp/Db/icb_hvps.db", "P=13LAB:,HVPS=hvps1,CARD=0,SERVER=icb/1,ADDR=2, LIMIT=1000")

#icbTcaSetup(serverName, maxModules, queueSize)
icbTcaSetup("icbTca/1", 10, 100)
#icbTcaConfig(serverName, module, ethernetAddress, icbAddress)
icbTcaConfig("icbTca/1", 0, 0x59e, 8)
dbLoadRecords("../../../mca/mcaApp/Db/icb_tca.db", "P=13LAB:,TCA=tca1,MCA=aim_adc2,CARD=0,SERVER=icbTca/1,ADDR=0")

iocInit()
