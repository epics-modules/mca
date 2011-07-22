# Example vxWorks startup file for SIS3820

< ../nfsCommands
< cdCommands

cd topbin
ld < SIS38XXTest.munch
cd startup

iocsh "st_SIS3801.iocsh"

