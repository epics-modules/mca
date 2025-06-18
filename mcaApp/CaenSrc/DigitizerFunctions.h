#include "ParamStruct.h"
#include <CAENDigitizerType.h>
#include <CAENDigitizer.h>
#include <cstdio>
#include <chrono> // Needed for get_time_ms (requires C++11)

int ProgramDigitizer(int handle, DigitizerParams_t Params, CAEN_DGTZ_DPP_PHA_Params_t DPPParams);

void SetDefaultParams(DigitizerParams_t * Params, CAEN_DGTZ_DPP_PHA_Params_t * DPPParams);

unsigned long long int get_time_ns();
