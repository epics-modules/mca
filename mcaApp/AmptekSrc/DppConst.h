
#pragma once

typedef enum ACQ_DEVICE_TYPE {
	/// MCA8000A uses PMCA COM API not supported in this application.
    devtypeMCA8000A,
	/// DP4 uses DPPAPI and DPP legacy protocol not supported in this application.
    devtypeDP4,
	/// PX4 uses DPPAPI and DPP legacy protocol not supported in this application.
    devtypePX4,
	/// DP5 (FW5) with DP4 emulation uses DPPAPI and DPP legacy protocol not supported in this application.
	devtypeDP4EMUL,
	/// DP5 (FW5) with Px4 emulation uses DPPAPI and DPP legacy protocol not supported in this application.
	devtypePX4EMUL,
	/// DP5 uses DPP new (this) protocol is supported in this application
	devtypeDP5,
	/// PX5 uses DPP new (this) protocol is supported in this application
	devtypePX5,
	/// DP5G uses DPP new (this) protocol is supported in this application
	devtypeDP5G,
	/// MCA8000D uses DPP new (this) protocol is supported in this application
	devtypeMCA8000D,
	/// TB5 uses DPP new (this) protocol is supported in this application ==4+5
	devtypeTB5,
	/// DP5X uses DPP new (this) protocol is supported in this application ==5+5
	devtypeDP5X
} acqDeviceType;

#define ACQ_DEVICE_TYPE_FIRST devtypeMCA8000A
#define ACQ_DEVICE_TYPE_LAST devtypeDP5X

typedef enum DP5_DPP_TYPES 
{
	/// DP5 uses DPP new (this) protocol is supported in this application ==0
	dppDP5,
	/// PX5 uses DPP new (this) protocol is supported in this application ==1
	dppPX5,
	/// DP5G uses DPP new (this) protocol is supported in this application ==2
	dppDP5G,
	/// MCA8000D uses DPP new (this) protocol is supported in this application ==3
	dppMCA8000D,
	/// TB5 uses DPP new (this) protocol is supported in this application ==4
	dppTB5,
	/// DP5X uses DPP new (this) protocol is supported in this application ==5
	dppDP5X
} dp5DppTypes;

#define DP5_DPP_TYPES_FIRST dppDP5
#define DP5_DPP_TYPES_LAST dppDP5X
