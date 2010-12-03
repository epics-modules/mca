# Example vxWorks startup file for SIS3820

< ../nfsCommands
< cdCommands

cd topbin
ld < mcaSISTest.munch

dbLoadDatabase("../../dbd/mcaSISTest.dbd",0,0)
mcaSISTest_registerRecordDeviceDriver(pdbbase) 

cd startup
< SIS3801_32.cmd
< SIS3820_32.cmd

< save_restore_sis.cmd

iocInit()

# save settings every thirty seconds
create_monitor_set("auto_settings_sis.req",30,"P=SIS:")
