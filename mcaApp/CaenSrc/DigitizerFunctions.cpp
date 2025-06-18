#include "DigitizerFunctions.h"

int ProgramDigitizer(int handle, DigitizerParams_t Params, CAEN_DGTZ_DPP_PHA_Params_t DPPParams){
    /* This function uses the CAENDigitizer API functions to perform the digitizer's initial configuration */
    //CAEN_DGTZ_ErrorCode ret = CAEN_DGTZ_Success;
    int ret = 0;

    /* Reset the digitizer */
    ret |= CAEN_DGTZ_Reset(handle);

    if (ret) {
        printf("ERROR: can't reset the digitizer.\n");
        return -1;
    }
    // I do not know what this does, but CAEN's example uses it
    // TODO: remove this eventually?
    ret |= CAEN_DGTZ_WriteRegister(handle, 0x8000, 0x01000114);  // Channel Control Reg (indiv trg, seq readout) ??

    ret |= CAEN_DGTZ_SetDPPAcquisitionMode(handle, Params.DPPAcqMode, Params.DPPSaveParam);
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle, Params.AcqMode);
    ret |= CAEN_DGTZ_SetRecordLength(handle, Params.RecordLength);
    ret |= CAEN_DGTZ_SetIOLevel(handle, Params.IOlev);
    ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle, Params.TrigMode);
    ret |= CAEN_DGTZ_SetChannelEnableMask(handle, Params.ChannelMask);
    ret |= CAEN_DGTZ_SetDPPEventAggregation(handle, Params.EventAggr, 0);
    // TODO: create parameter for this
    ret |= CAEN_DGTZ_SetRunSynchronizationMode(handle, CAEN_DGTZ_RUN_SYNC_Disabled);
    ret |= CAEN_DGTZ_SetDPPParameters(handle, Params.ChannelMask, &DPPParams);

    for(unsigned int i=0; i<MAX_DPP_PHA_CHANNEL_SIZE; i++) {
        if (Params.ChannelMask & (1<<i)) {
            ret |= CAEN_DGTZ_SetChannelDCOffset(handle, i, Params.DCOffset[i]);
            ret |= CAEN_DGTZ_SetDPPPreTriggerSize(handle, i, Params.PreTriggerSize[i]);
            ret |= CAEN_DGTZ_SetChannelPulsePolarity(handle, i, Params.PulsePolarity[i]);
        }
    }

    // TODO: Implement virtual probe selection
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, ANALOG_TRACE_1, CAEN_DGTZ_DPP_VIRTUALPROBE_Delta2);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, ANALOG_TRACE_2, CAEN_DGTZ_DPP_VIRTUALPROBE_None);
    //ret |= CAEN_DGTZ_SetDPP_VirtualProbe(handle, DIGITAL_TRACE_1, CAEN_DGTZ_DPP_DIGITALPROBE_Peaking);

    if (ret) {
        printf("Warning: errors found during the programming of the digitizer.\nSome settings may not be executed\n");
        return ret;
    } else {
        return 0;
    }
}
void SetDefaultParams(DigitizerParams_t * Params, CAEN_DGTZ_DPP_PHA_Params_t * DPPParams){
    Params->IOlev = CAEN_DGTZ_IOLevel_NIM;
    Params->DPPAcqMode = CAEN_DGTZ_DPP_ACQ_MODE_List; // TODO: Maybe consider CAEN_DGTZ_DPP_ACQ_MODE_Mixed
    Params->DPPSaveParam = CAEN_DGTZ_DPP_SAVE_PARAM_EnergyAndTime;
    Params->AcqMode = CAEN_DGTZ_SW_CONTROLLED;
    Params->RecordLength = 50000; // Only needed for Mixed/Scope mode
    Params->ChannelMask = (1<<8) - 1; // Enable 8 channels
//    Params->ChannelMask = (1<<MAX_DPP_PHA_CHANNEL_SIZE) - 1; // Enable all channels
    Params->EventAggr = 0; // 0 = Auto
    Params->TrigMode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;

    for(unsigned int ch = 0; ch < MAX_DPP_PHA_CHANNEL_SIZE; ch++) {
        Params->PulsePolarity[ch] = CAEN_DGTZ_PulsePolarityPositive;
        Params->PreTriggerSize[ch] = 1000;
        Params->DCOffset[ch] = 0x8000;
        DPPParams->thr[ch] = 100;   // Trigger Threshold (in LSB)
        DPPParams->k[ch] = 3000;     // Trapezoid Rise Time (ns) 
        DPPParams->m[ch] = 900;      // Trapezoid Flat Top  (ns) 
        DPPParams->M[ch] = 25000;      // Decay Time Constant (ns) 
        DPPParams->ftd[ch] = 500;    // Flat top delay (peaking time) (ns) 
        DPPParams->a[ch] = 4;       // Trigger Filter smoothing factor (number of samples to average for RC-CR2 filter) Options: 1; 2; 4; 8; 16; 32
        DPPParams->b[ch] = 200;     // Input Signal Rise time (ns) 
        DPPParams->trgho[ch] = 1200;  // Trigger Hold Off
        DPPParams->nsbl[ch] = 4;     //number of samples for baseline average calculation. Options: 1->16 samples; 2->64 samples; 3->256 samples; 4->1024 samples; 5->4096 samples; 6->16384 samples
        DPPParams->nspk[ch] = 0;     //Peak mean (number of samples to average for trapezoid height calculation). Options: 0-> 1 sample; 1->4 samples; 2->16 samples; 3->64 samples
        DPPParams->pkho[ch] = 2000;  //peak holdoff (ns)
        DPPParams->blho[ch] = 500;   //Baseline holdoff (ns)
        DPPParams->enf[ch] = 1.0; // Energy Normalization Factor
        DPPParams->decimation[ch] = 0;  //decimation (the input signal samples are averaged within this number of samples): 0 ->disabled; 1->2 samples; 2->4 samples; 3->8 samples
        DPPParams->dgain[ch] = 0;    //decimation gain. Options: 0->DigitalGain=1; 1->DigitalGain=2 (only with decimation >= 2samples); 2->DigitalGain=4 (only with decimation >= 4samples); 3->DigitalGain=8( only with decimation = 8samples).
        DPPParams->otrej[ch] = 0; // Deprecated
        DPPParams->trgwin[ch] = 0;  //Enable Rise time Discrimination. Options: 0->disabled; 1->enabled
        DPPParams->twwdt[ch] = 100;  //Rise Time Validation Window (ns)
                    //DPPParams.tsampl[ch] = 10;
                    //DPPParams.dgain[ch] = 1;
    }
}

unsigned long long int get_time_ns(){
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
