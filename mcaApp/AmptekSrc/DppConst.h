
#pragma once

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
