# Example vxWorks startup file
#Following must be added for many board support packages

cd "/corvette/home/epics/3.14/mca/iocBoot/iocVxWorks"
< ../nfsCommands
< cdCommands

cd topbin
ld < mcaAIM.munch

cd startup
dbLoadDatabase("../../dbd/mcaAIM.dbd",0,0)
registerRecordDeviceDriver(pdbbase) 

dbLoadRecords("../../mcaApp/Db/mca.db","P=mcaTest:,M=aim_adc1,NCHAN=2048,NUSE=2048,DTYPE=MPF MCA,INP=#C0 S0@AIMServ")
dbLoadRecords("../../mcaApp/Db/icb_adc.db", "P=mcaTest:,ADC=adc1,CARD=0,SERVER=icb/1,ADDR=0")
dbLoadRecords("../../mcaApp/Db/icb_tca.db", "P=mcaTest:,TCA=tca1,MCA=aim_adc1,CARD=0,SERVER=icbTca/1,ADDR=0")

#
routerInit
localMessageRouterStart(0)

#aimDebug=9
#mcaAIMServerDebug=10
#mcaRecordDebug=0
#devMcaMpfDebug=0
#transformRecordDebug = 0
#swaitRecordDebug=10

#AIMConfig(serverName, ethernetAddress, ADC port, maxChans,
#          maxSignals, maxSequences, ethernetDevice, queueSize)
AIMConfig("AIMServ", 0x59e, 1, 2048, 1, 1, "ei0", 1000)
nmc_show_modules

#icbConfig(serverName, maxModules, address, queueSize)
icbConfig("icb/1", 10, "NI59E:5", 100)

#icbTcaConfig(serverName, maxModules, address, queueSize)
icbTcaConfig("icbTca/1", 1, "NI59E:8", 100)

iocInit()
