STR7201Setup(1,0x90000000,220,6)
# STR7201Config(card, maxSignals, maxChans, ch1RefEnable)
STR7201Config(0, 32, 2048, 1)
dbLoadRecords("$(MCA)/mcaApp/Db/Struck32.db","P=SIS:3801:")
dbLoadTemplate("SIS3801_32.template")

/* Scaler record */
dbLoadRecords("$(MCA)/mcaApp/Db/STR7201scaler.db", "P=SIS:,S=scaler1,C=0")

