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
dbLoadTemplate("SIS3820_32.substitutions")

/* Scaler record with asyn device support*/
dbLoadRecords("$(MCA)/mcaApp/Db/SIS3820AsynScaler.db", "P=SIS:3820:,S=scaler1,OUT=@asyn(mcaSIS3820/1 0)")

#SIS3820Setup(numCards,
#             baseAddress,
#             intVector,
#             intLevel")
#SIS3820Setup(1, 0x94000000, 224, 6)

#SIS3820Config(board,
#              maxSignals,
#              maxChans,
#              ch1RefEnable,
#              softAdvance,
#              inputMode,
#              outputMode,
#              lnePrescale)
#SIS3820Config(0, 32, 2048, 1, 0, 2, 0, 1000000)

/* Scaler record */
#dbLoadRecords("$(MCA)/mcaApp/Db/SIS3820Scaler.db", "P=SIS:3820:,S=scaler1,C=0")

#asynSetTraceIOMask("mcaSIS3820/1",0,2)
#asynSetTraceMask  ("mcaSIS3820/1",0,255)

