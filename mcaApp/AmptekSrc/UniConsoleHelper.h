/** Universal interface version of CConsoleHelper
 */

#pragma once

#include "ConsoleHelper.h"      // Shareable library configuration
#include "DppLibUsb.h"          // LibUsb Support

typedef enum {
    DppInterfaceEthernet,
    DppInterfaceUSB,
    DppInterfaceSerial
} DppInterface_t;

class IMPORT_EXPORT CUniConsoleHelper : public CConsoleHelper
{
public:
    CUniConsoleHelper(void);
    ~CUniConsoleHelper(void);

    /// Interface-independent variables and methods
    bool isConnected;
    int NumDevices;
    DppInterface_t interfaceType;
    bool ConnectDpp(DppInterface_t interfaceType, const char *addressInfo);
    void Close_Connection();
    bool SendCommand(TRANSMIT_PACKET_TYPE XmtCmd);
    bool SendCommand_Config(TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions);

    /// LibUsb communications class.
    CDppLibUsb DppLibUsb;
    /// LibUsb is connected if true.
    bool LibUsb_isConnected;
    /// LibUsb number of devices found.
    int  LibUsb_NumDevices;
    /// LibUsb connect to the default DPP.
    bool LibUsb_Connect_Default_DPP();
    /// LibUsb close the current connection.
    void LibUsb_Close_Connection();
    /// LibUsb send a command that does not require additional processing.
    bool LibUsb_SendCommand(TRANSMIT_PACKET_TYPE XmtCmd);
    /// LibUsb send a command that requires configuration options processing.
    bool LibUsb_SendCommand_Config(TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions);
    ///  LibUsb receive data.
    bool LibUsb_ReceiveData();

    /// Processes DPP data from all communication interfaces (USB,RS232,INET)
    bool ReceiveData();
};
