#pragma once

#ifdef _WIN32
	#define LIBUSB_WINUSB_BACKEND 1
#endif

#ifdef LIBUSB_WINUSB_BACKEND 
	#include "libusb.h"
	#ifdef _MSC_VER
		#pragma comment(lib, ".\\DeviceIO\\libusb.lib")
	#endif
#else
	#include <libusb.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#define AMPTEK_DP5_VENDOR_ID 0x10C4
#define AMPTEK_DP5_PRODUCT_ID 0x842A

#define INTERFACE_NUMBER 0
#define TIMEOUT_MS 5000
#define BULK_IN_ENDPOINT 0x81
#define BULK_OUT_ENDPOINT 0x02
#define MAX_BULK_IN_TRANSFER_SIZE 32768
#define MAX_BULK_OUT_TRANSFER_SIZE 520
#define STATUS_PACKET_SIZE 64
#define DP5_USB_TIMEOUT 500         // default timeout (500mS)
#define DP5_DIAGDATA_TIMEOUT 2500	// diag data timeout
#define PID1_REQ_SCOPE_MISC_TO 0x03
#define PID2_SEND_DIAGNOSTIC_DATA_TO 0x05


class CDppLibUsb
{
public:
	CDppLibUsb(void);
	~CDppLibUsb(void);

	unsigned char data_in[MAX_BULK_IN_TRANSFER_SIZE];		// data bytes in
	unsigned char data_out[MAX_BULK_OUT_TRANSFER_SIZE];		// data bytes out

	libusb_device_handle *DppLibusbHandle;
	int NumDevices;
	int CurrentDevice;
	bool bLibusbReady;
	bool bDeviceReady;
	char LastLibUsbError[256];
	bool bDeviceConnected;

	int InitializeLibusb();
	void DeinitializeLibusb();
	libusb_device_handle * FindUSBDevice(int idxAmptekDevice);
	void CloseUSBDevice(libusb_device_handle * devh);
	int SendPacketUSB(libusb_device_handle *devh, unsigned char data_out[], unsigned char data_in[]);
	bool isAmptekDP5Device(libusb_device_descriptor desc);
	int CountDP5LibusbDevices();
	void PrintDevices();

#ifndef LIBUSB_WINUSB_BACKEND
	const char * libusb_strerror(enum libusb_error error_code);
#endif

};









