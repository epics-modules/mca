
#include "UniConsoleHelper.h"
#include "stringex.h"
//#pragma warning(disable:4309)
#include <string.h>

CUniConsoleHelper::CUniConsoleHelper(void) : CConsoleHelper()
{
    DppLibUsb.NumDevices = 0;
    LibUsb_isConnected = false;
    LibUsb_NumDevices = 0;
}

CUniConsoleHelper::~CUniConsoleHelper(void)
{
}

bool CUniConsoleHelper::ConnectDpp(DppInterface_t ifaceType, const char *addressInfo)
{
    interfaceType = ifaceType;
    bool status = false;
    
    isConnected = false;
    NumDevices = 0;
    switch(interfaceType) {
        case DppInterfaceEthernet:
            status = DppSocket_Connect_Default_DPP((char *)addressInfo);
            isConnected = DppSocket_isConnected;
            NumDevices = DppSocket_NumDevices;
            break;
        
        case DppInterfaceUSB:
            status = LibUsb_Connect_Default_DPP();
            isConnected = LibUsb_isConnected;
            NumDevices = LibUsb_NumDevices;
           break;
        
        case DppInterfaceSerial:
            break;
            
        default:
            break;
    }
    return status;
}

void CUniConsoleHelper::Close_Connection()
{
    switch(interfaceType) {
        case DppInterfaceEthernet:
            DppSocket_Close_Connection();
            break;
        
        case DppInterfaceUSB:
            LibUsb_Close_Connection();
            break;
        
        case DppInterfaceSerial:
            break;
            
        default:
            break;
    }
}

bool CUniConsoleHelper::SendCommand(TRANSMIT_PACKET_TYPE command)
{
    switch(interfaceType) {
        case DppInterfaceEthernet:
            return DppSocket_SendCommand(command);
            break;
        
        case DppInterfaceUSB:
            return LibUsb_SendCommand(command);
            break;
        
        case DppInterfaceSerial:
            return false;
            break;
        
        default:
            return false;
            break;
    }
}

bool CUniConsoleHelper::SendCommand_Config(TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions)
{
    switch(interfaceType) {
        case DppInterfaceEthernet:
            return DppSocket_SendCommand_Config(XmtCmd, CfgOptions);
            break;
        
        case DppInterfaceUSB:
            return LibUsb_SendCommand_Config(XmtCmd, CfgOptions);
            break;
        
        case DppInterfaceSerial:
            return false;
            break;
            
        default:
            return false;
            break;
    }
}

bool CUniConsoleHelper::ReceiveData()
{
  if (!isConnected) return false;
  return CConsoleHelper::ReceiveData();
}

bool CUniConsoleHelper::LibUsb_Connect_Default_DPP()
{

    // flag is for notifications, if already connect will return dusbStatNoAction
    // use bDeviceConnected to detect connection
    DppLibUsb.InitializeLibusb();
    LibUsb_isConnected = false;
    LibUsb_NumDevices = 0;
    DppLibUsb.NumDevices = DppLibUsb.CountDP5LibusbDevices();
    if (DppLibUsb.NumDevices > 1) {
        // choose dpp device here or default to first device
    } else if (DppLibUsb.NumDevices == 1) {
        // default to first device
    } else {
    }
    if (DppLibUsb.NumDevices > 0) {
        DppLibUsb.CurrentDevice = 1;    // set to default device
        DppLibUsb.DppLibusbHandle = DppLibUsb.FindUSBDevice(DppLibUsb.CurrentDevice); //connect
        if (DppLibUsb.bDeviceConnected) { // connection detected
            LibUsb_isConnected = true;
            LibUsb_NumDevices = DppLibUsb.NumDevices;
        }
    } else {
        LibUsb_isConnected = false;
        LibUsb_NumDevices = 0;
    }
    return (LibUsb_isConnected);
}

void CUniConsoleHelper::LibUsb_Close_Connection()
{
    if (DppLibUsb.bDeviceConnected) { // clean-up: close usb connection
        DppLibUsb.bDeviceConnected = false;
        DppLibUsb.CloseUSBDevice(DppLibUsb.DppLibusbHandle);
        LibUsb_isConnected = false;
        LibUsb_NumDevices = 0;
        DppLibUsb.DeinitializeLibusb();
    }
}

bool CUniConsoleHelper::LibUsb_SendCommand(TRANSMIT_PACKET_TYPE XmtCmd)
{
    bool bHaveBuffer;
    int bSentPkt;
    bool bMessageSent;

    bMessageSent = false;
    if (DppLibUsb.bDeviceConnected) { 
        memset(&DP5Proto.BufferOUT[0],0,sizeof(DP5Proto.BufferOUT));
        bHaveBuffer = (bool) SndCmd.DP5_CMD(DP5Proto.BufferOUT, XmtCmd);
        if (bHaveBuffer) {
            bSentPkt = DppLibUsb.SendPacketUSB(DppLibUsb.DppLibusbHandle, DP5Proto.BufferOUT, DP5Proto.PacketIn);
            if (bSentPkt) {
                bMessageSent = true;
            }
        }
    }
    return (bMessageSent);
}

// COMMAND (CONFIG_OPTIONS Needed)
//                    Command Description
//
// XMTPT_SEND_CONFIG_PACKET_TO_HW (HwCfgDP5Out,SendCoarseFineGain,PC5_PRESENT,DppType)
//                    Processes a configuration string for errors before sending
//                            Allows only one gain setting type (coarse,fine OR total gain)
//                            Removes commands from string that should not be sent to device type
// XMTPT_SEND_CONFIG_PACKET_EX (HwCfgDP5Out)
//                    Sends the a configuration with minimal error checking
// XMTPT_FULL_READ_CONFIG_PACKET (PC5_PRESENT,DppType)
//                    Creates and sends a full readback command
bool CUniConsoleHelper::LibUsb_SendCommand_Config(TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions)
{
    bool bHaveBuffer;
    int bSentPkt;
    bool bMessageSent;

    bMessageSent = false;
    if (DppLibUsb.bDeviceConnected) {
        memset(&DP5Proto.BufferOUT[0],0,sizeof(DP5Proto.BufferOUT));
        bHaveBuffer = (bool) SndCmd.DP5_CMD_Config(DP5Proto.BufferOUT, XmtCmd, CfgOptions);
        if (bHaveBuffer) {
            bSentPkt = DppLibUsb.SendPacketUSB(DppLibUsb.DppLibusbHandle, DP5Proto.BufferOUT, DP5Proto.PacketIn);
            if (bSentPkt) {
                bMessageSent = true;
            }
        }
    }
    return (bMessageSent);
}

bool CUniConsoleHelper::LibUsb_ReceiveData()
{
    bool bDataReceived;

    bDataReceived = true;
    if (DppLibUsb.bDeviceConnected) { 
        bDataReceived = ReceiveData();
    }
    return (bDataReceived);
}

