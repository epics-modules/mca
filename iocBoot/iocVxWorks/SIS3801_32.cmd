# STR7201Setup(int maxCards, int baseAddress, int intVector, int intLevel);
STR7201Setup(1,0x90000000,220,6)
# STR7201Config(int card, int maxSignals, int maxChans, int ch1RefEnable, int softAdvance);
STR7201Config(0, 32, 2048, 1, 0)
dbLoadRecords("$(MCA)/mcaApp/Db/Struck32.db","P=SIS:3801:")
dbLoadTemplate("SIS3801_32.substitutions")

/* Scaler record */
dbLoadRecords("$(MCA)/mcaApp/Db/STR7201scaler.db", "P=SIS:3801:,S=scaler1,C=0")

