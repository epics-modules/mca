#include "CAENDigitizerType.h"

#ifndef PARAM_STRUCTS
#define PARAM_STRUCTS
typedef struct
{
    CAEN_DGTZ_ConnectionType LinkType;
    uint32_t VMEBaseAddress;
    uint32_t LinkNum;
    uint32_t ConetNode;
    uint32_t RecordLength;
    uint32_t ChannelMask;
    int EventAggr;
    CAEN_DGTZ_PulsePolarity_t PulsePolarity[MAX_DPP_PHA_CHANNEL_SIZE];
    uint32_t DCOffset[MAX_DPP_PHA_CHANNEL_SIZE];
    uint32_t PreTriggerSize[MAX_DPP_PHA_CHANNEL_SIZE];
    CAEN_DGTZ_AcqMode_t AcqMode;
    CAEN_DGTZ_DPP_SaveParam_t DPPSaveParam;
    CAEN_DGTZ_DPP_AcqMode_t DPPAcqMode;
    CAEN_DGTZ_TriggerMode_t TrigMode;
    CAEN_DGTZ_IOLevel_t IOlev;
} DigitizerParams_t;

#endif
