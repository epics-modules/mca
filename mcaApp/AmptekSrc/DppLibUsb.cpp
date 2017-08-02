#include "DppLibUsb.h"

CDppLibUsb::CDppLibUsb(void)
{

}
CDppLibUsb::~CDppLibUsb(void)
{

}

// InitializeLibusb must be call before any other libusb operations
int CDppLibUsb::InitializeLibusb()
{
	int iStatus=0;
	iStatus = libusb_init(NULL);
	if (iStatus != 0) {
		fprintf(stderr, "Unable to initialize libusb. %s\n", libusb_strerror((libusb_error)iStatus));
	}
	return iStatus;
}

// DeinitializeLibusb must be called after all libusb operations 
// have been completed and all devices/lists are closed
void CDppLibUsb::DeinitializeLibusb()
{
	libusb_exit(NULL);
}

// devices start at 1
// Do only when no devices are connected
libusb_device_handle * CDppLibUsb::FindUSBDevice(int idxAmptekDevice)
{
	struct libusb_device **devs;
	struct libusb_device *found = NULL;
	struct libusb_device_handle *handle = NULL;
	struct libusb_device_descriptor desc;
	struct libusb_device *device = NULL;
	ssize_t devcnt = 0;
	ssize_t iDev = 0;
	int r;
	int idxDev=1;

	bDeviceConnected = false;
	devcnt = libusb_get_device_list(NULL, &devs);
	if (devcnt < 0) {
		fprintf(stderr, "failed to get device list");
		return NULL;
	}
	for (iDev = 0; iDev < devcnt; iDev++) {
		device = devs[iDev];
		int r = libusb_get_device_descriptor (device, &desc);
		if (r < 0) {
			fprintf(stderr, libusb_strerror((libusb_error)r));
			goto out;
			//continue;
		}
		if (isAmptekDP5Device(desc)) {
			if (idxAmptekDevice==idxDev) {
				found = device;
				break;
			} else {	// inc counter to next device
				idxDev++;
			}
		}
	}
	if (found) {
		r = libusb_open(found, &handle);
		if (r < 0) {
			fprintf(stderr, libusb_strerror((libusb_error)r));
			handle = NULL;
		} else {
			if (handle != NULL) {
				r = libusb_claim_interface(handle, INTERFACE_NUMBER);
				if (r >= 0) {
					bDeviceConnected = true; // have interface
				} else {
					fprintf(stderr, "libusb_claim_interface error %s\n", libusb_strerror((libusb_error)r));
				}
			} else {
				fprintf(stderr, "Unable to find a dpp device.\n");
			}
		}
	}

out:
	libusb_free_device_list(devs, 1);
	return handle;
}

void CDppLibUsb::CloseUSBDevice(libusb_device_handle * devh) 
{
	int r;
	if (devh != NULL) {
		r = libusb_release_interface(devh, INTERFACE_NUMBER);
		if (r < 0) {
			fprintf(stderr, "libusb_release_interface error %s\n", libusb_strerror((libusb_error)r));
		}
		libusb_close(devh);
	}
}

int CDppLibUsb::SendPacketUSB(libusb_device_handle *devh, unsigned char data_out[], unsigned char data_in[])
{
	int bytes_transferred;
	unsigned int timeout = 5000;
	int result = 0;
	int length = 0; 
	
	if ((data_out[2] == PID1_REQ_SCOPE_MISC_TO) && data_out[3] == PID2_SEND_DIAGNOSTIC_DATA_TO) {
		timeout = DP5_DIAGDATA_TIMEOUT;
	} else {
		timeout = DP5_USB_TIMEOUT;
	}

	length = data_out[4];
	length *= 256;
	length += data_out[5];
	length += 8;

	result = libusb_bulk_transfer(devh, BULK_OUT_ENDPOINT, data_out, length, &bytes_transferred, timeout);
	if (result >= 0) {
	  	result = libusb_bulk_transfer(devh, BULK_IN_ENDPOINT, data_in, MAX_BULK_IN_TRANSFER_SIZE, &bytes_transferred, timeout);
		if (result >= 0) {
			if (bytes_transferred > 0) {
				return bytes_transferred;
			} else {
				fprintf(stderr, "No data received in bulk transfer (%d)\n", result);
				return -1;
			}
		} else {
			fprintf(stderr, "Error receiving data via bulk transfer %d\n", result);
			return result;
		}
	} else {
		fprintf(stderr, "Error sending data via bulk transfer %d\n", result);
		return result;
	}
  	return 0;
 }

bool CDppLibUsb::isAmptekDP5Device(libusb_device_descriptor desc)
{
	bool isDevice=false;
	if ((desc.idVendor == AMPTEK_DP5_VENDOR_ID) && (desc.idProduct == AMPTEK_DP5_PRODUCT_ID))
	{
		isDevice=true;
	}
	return isDevice;
}

// Counts number of dpp devices
// Do only when no devices are connected
int CDppLibUsb::CountDP5LibusbDevices()
{
	libusb_device **devs;
	int iDppCount=0;

	ssize_t cnt = libusb_get_device_list (NULL, &devs); 
	if (cnt < 0)
		return (-1);
	int nr = 0, i = 0;
	struct libusb_device_descriptor desc;
	for (i = 0; i < cnt; ++i)
	{
		int r = libusb_get_device_descriptor (devs[i], &desc);
		if (r != 0) {
			fprintf(stderr, libusb_strerror((libusb_error)r));
			continue;
		}
		if (isAmptekDP5Device(desc)) { nr++; }
	}
	libusb_free_device_list (devs, 1);
	iDppCount = (int)nr;
	return iDppCount;
}

// Do only when no devices are connected
void CDppLibUsb::PrintDevices()
{
	struct libusb_device **devs;
	struct libusb_device *dev = NULL;
	int i = 0;
	ssize_t cnt = 0;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0) {
		fprintf(stderr, "failed to get device list");
		return;
	}

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			fprintf(stderr, "device descriptor error: %s", libusb_strerror((libusb_error)r));
			return;
		}
		if (isAmptekDP5Device(desc)) {
			printf("%04x:%04x (bus %d, device %d)\n", desc.idVendor, desc.idProduct, libusb_get_bus_number(dev), libusb_get_device_address(dev));
		}
	}
	libusb_free_device_list(devs, 1);
}

#ifndef LIBUSB_WINUSB_BACKEND
const char * CDppLibUsb::libusb_strerror(enum libusb_error error_code)
{
	switch (error_code) {
	case LIBUSB_SUCCESS:
		return "Success";
	case LIBUSB_ERROR_IO:
		return "Input/output error";
	case LIBUSB_ERROR_INVALID_PARAM:
		return "Invalid parameter";
	case LIBUSB_ERROR_ACCESS:
		return "Access denied (insufficient permissions)";
	case LIBUSB_ERROR_NO_DEVICE:
		return "No such device (it may have been disconnected)";
	case LIBUSB_ERROR_NOT_FOUND:
		return "Entity not found";
	case LIBUSB_ERROR_BUSY:
		return "Resource busy";
	case LIBUSB_ERROR_TIMEOUT:
		return "Operation timed out";
	case LIBUSB_ERROR_OVERFLOW:
		return "Overflow";
	case LIBUSB_ERROR_PIPE:
		return "Pipe error";
	case LIBUSB_ERROR_INTERRUPTED:
		return "System call interrupted (perhaps due to signal)";
	case LIBUSB_ERROR_NO_MEM:
		return "Insufficient memory";
	case LIBUSB_ERROR_NOT_SUPPORTED:
		return "Operation not supported or unimplemented on this platform";
	case LIBUSB_ERROR_OTHER:
		return "Other error";
	}
	return "Unknown error";
}
#endif
