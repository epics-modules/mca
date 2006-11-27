#mcaSIS3820Setup(int baseAddress, int intVector, int intLevel)
mcaSIS3820Setup(0x94000000, 224, 6)
#mcaSIS3820Config("Port name",
#                  card,
#                  channels,
#                  signals,
#                  input mode,
#                  output mode,
#                  lne prescale)
mcaSIS3820Config("mcaSIS3820/1", 0, 2048, 32, 2, 0, 1000000)
dbLoadTemplate("SIS3820_32.substitutions")

#asynSetTraceIOMask("mcaSIS3820/1",0,2)
#asynSetTraceMask  ("mcaSIS3820/1",0,255)

