#mcaSIS3820Setup(int baseAddress, int intVector, int intLevel)
mcaSIS3820Setup(0x94000000, 224, 6)
#mcaSIS3820Config("Port name",
#					card,
#					channels,
#					signals,
#					input mode,
#					output mode,
#					lne prescale)
mcaSIS3820Config("mcaSIS3820/1", 0, 2048, 32, 2, 0, 1000000)
dbLoadRecords("$(MCA)/mcaApp/Db/SIS3820.db", "P=SIS:3820:, INP=@asyn(mcaSIS3820/1 0)")
dbLoadRecords("$(MCA)/mcaApp/Db/Struck32.db","P=SIS:3820:")
dbLoadTemplate("SIS3820_32.template")

#asynSetTraceIOMask("mcaSIS3820/1",0,2)
#asynSetTraceMask  ("mcaSIS3820/1",0,255)

