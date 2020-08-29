
#include "DP5Status.h"
#include "stringex.h"
#include "DppConst.h"
#ifdef _MSC_VER
    #pragma warning(disable:4996)
#endif

CDP5Status::CDP5Status(void)
{
}

CDP5Status::~CDP5Status(void)
{
}

void CDP5Status::Process_Status(DP4_FORMAT_STATUS *m_DP5_Status)
{
	bool bDMCA_LiveTime = false;
	unsigned int uiFwBuild = 0;

	m_DP5_Status->DEVICE_ID = m_DP5_Status->RAW[39];
    m_DP5_Status->FastCount = DppUtil.LongWordToDouble(0, m_DP5_Status->RAW);
    m_DP5_Status->SlowCount = DppUtil.LongWordToDouble(4, m_DP5_Status->RAW);
    m_DP5_Status->GP_COUNTER = DppUtil.LongWordToDouble(8, m_DP5_Status->RAW);
    m_DP5_Status->AccumulationTime = (float)m_DP5_Status->RAW[12] * 0.001 + (float)(m_DP5_Status->RAW[13] + (float)m_DP5_Status->RAW[14] * 256.0 + (float)m_DP5_Status->RAW[15] * 65536.0) * 0.1;
	m_DP5_Status->RealTime = ((double)m_DP5_Status->RAW[20] + ((double)m_DP5_Status->RAW[21] * 256.0) + ((double)m_DP5_Status->RAW[22] * 65536.0) + ((double)m_DP5_Status->RAW[23] * 16777216.0)) * 0.001;

	m_DP5_Status->Firmware = m_DP5_Status->RAW[24];
    m_DP5_Status->FPGA = m_DP5_Status->RAW[25];
	
	if (m_DP5_Status->Firmware > 0x65) {
		m_DP5_Status->Build = m_DP5_Status->RAW[37] & 0xF;		//Build # added in FW6.06
	} else {
		m_DP5_Status->Build = 0;
	}

	//Firmware Version:  6.07  Build:  0 has LiveTime and PREL
	//DEVICE_ID 0=DP5,1=PX5,2=DP5G,3=MCA8000D,4=TB5
	if (m_DP5_Status->DEVICE_ID == dppMCA8000D) {
		if (m_DP5_Status->Firmware >= 0x67) {
			bDMCA_LiveTime = true;
		}
	}

	if (bDMCA_LiveTime) {
		m_DP5_Status->LiveTime = ((double)m_DP5_Status->RAW[16] + ((double)m_DP5_Status->RAW[17] * 256.0) + ((double)m_DP5_Status->RAW[18] * 65536.0) + ((double)m_DP5_Status->RAW[19] * 16777216.0)) * 0.001;
	} else {
		m_DP5_Status->LiveTime = 0;
	}

    if (m_DP5_Status->RAW[29] < 128) {
        m_DP5_Status->SerialNumber = (unsigned long)DppUtil.LongWordToDouble(26, m_DP5_Status->RAW);
    } else {
        m_DP5_Status->SerialNumber = -1;
    }
    
	// m_DP5_Status->HV = (double)(m_DP5_Status->RAW[31] + (m_DP5_Status->RAW[30] & 15) * 256) * 0.5;					// 0.5V/count

	if (m_DP5_Status->RAW[30] < 128)  {    // not negative
        m_DP5_Status->HV = ((double)m_DP5_Status->RAW[31] + ((double)m_DP5_Status->RAW[30] * 256.0)) * 0.5;  // 0.5V/count
	} else {
		m_DP5_Status->HV = (((double)m_DP5_Status->RAW[31] + ((double)m_DP5_Status->RAW[30] * 256)) - 65536.0) * 0.5; // 0.5V/count
	}
      
	m_DP5_Status->DET_TEMP = (double)((m_DP5_Status->RAW[33]) + (m_DP5_Status->RAW[32] & 15) * 256) * 0.1; // - 273.16;		// 0.1K/count
    m_DP5_Status->DP5_TEMP = m_DP5_Status->RAW[34] - ((m_DP5_Status->RAW[34] & 128) * 2);

    m_DP5_Status->PresetRtDone = ((m_DP5_Status->RAW[35] & 128) == 128);

	//unsigned char:35 BIT:D6 
	// == Preset LiveTime Done for MCA8000D
	// == FAST Thresh locked for other dpp devices
	m_DP5_Status->PresetLtDone = false;
	m_DP5_Status->AFAST_LOCKED = false;
	if (bDMCA_LiveTime) {		// test for MCA8000D
		m_DP5_Status->PresetLtDone = ((m_DP5_Status->RAW[35] & 64) == 64);
	} else {
		m_DP5_Status->AFAST_LOCKED = ((m_DP5_Status->RAW[35] & 64) == 64);
	}
    m_DP5_Status->MCA_EN = ((m_DP5_Status->RAW[35] & 32) == 32);
    m_DP5_Status->PRECNT_REACHED = ((m_DP5_Status->RAW[35] & 16) == 16);
    m_DP5_Status->SCOPE_DR = ((m_DP5_Status->RAW[35] & 4) == 4);
    m_DP5_Status->DP5_CONFIGURED = ((m_DP5_Status->RAW[35] & 2) == 2);

    m_DP5_Status->AOFFSET_LOCKED = ((m_DP5_Status->RAW[36] & 128) == 0);  // 0=locked, 1=searching
    m_DP5_Status->MCS_DONE = ((m_DP5_Status->RAW[36] & 64) == 64);

    m_DP5_Status->b80MHzMode = ((m_DP5_Status->RAW[36] & 2) == 2);
    m_DP5_Status->bFPGAAutoClock = ((m_DP5_Status->RAW[36] & 1) == 1);

    m_DP5_Status->PC5_PRESENT = ((m_DP5_Status->RAW[38] & 128) == 128);
    if  (m_DP5_Status->PC5_PRESENT) {
        m_DP5_Status->PC5_HV_POL = ((m_DP5_Status->RAW[38] & 64) == 64);
        m_DP5_Status->PC5_8_5V = ((m_DP5_Status->RAW[38] & 32) == 32);
    } else {
        m_DP5_Status->PC5_HV_POL = false;
        m_DP5_Status->PC5_8_5V = false;
    }

	if (m_DP5_Status->Firmware >= 0x65) {		// reboot flag added FW6.05
		if ((m_DP5_Status->RAW[36] & 32) == 32) {
            m_DP5_Status->ReBootFlag = true;
		} else {
			m_DP5_Status->ReBootFlag = false;
		}
	} else {
        m_DP5_Status->ReBootFlag = false;
	}

	m_DP5_Status->TEC_Voltage = (((double)(m_DP5_Status->RAW[40] & 15) * 256.0) + (double)(m_DP5_Status->RAW[41])) / 758.5;
	m_DP5_Status->DPP_ECO = m_DP5_Status->RAW[49];
	m_DP5_Status->bScintHas80MHzOption = false;
	m_DP5_Status->DPP_options = (m_DP5_Status->RAW[42] & 15);
	m_DP5_Status->HPGe_HV_INH = false;
	m_DP5_Status->HPGe_HV_INH_POL = false;
	m_DP5_Status->AU34_2 = false;
	m_DP5_Status->isAscInstalled = false;
	m_DP5_Status->isDP5_RevDxGains = false;
	if (m_DP5_Status->DEVICE_ID == dppPX5) {
		if (m_DP5_Status->DPP_options == PX5_OPTION_HPGe_HVPS) {
			m_DP5_Status->HPGe_HV_INH = ((m_DP5_Status->RAW[42] & 32) == 32);
			m_DP5_Status->HPGe_HV_INH_POL = ((m_DP5_Status->RAW[42] & 16) == 16);
			if (m_DP5_Status->DPP_ECO == 1) {
				m_DP5_Status->isAscInstalled = true;
				m_DP5_Status->AU34_2 = ((m_DP5_Status->RAW[42] & 64) == 64);
			}
		}
	} else if ((m_DP5_Status->DEVICE_ID == dppDP5G) || (m_DP5_Status->DEVICE_ID == dppTB5)) {
		if ((m_DP5_Status->DPP_ECO == 1) || (m_DP5_Status->DPP_ECO == 2)) {
			// DPP_ECO == 2 80MHz option added 20150409
			m_DP5_Status->bScintHas80MHzOption = true;
		}
	} else if (m_DP5_Status->DEVICE_ID == dppDP5) {
		uiFwBuild = m_DP5_Status->Firmware;
		uiFwBuild = uiFwBuild << 8;
		uiFwBuild = uiFwBuild + m_DP5_Status->Build;
		// uiFwBuild - firmware with build for comparison
		if (uiFwBuild >= 0x686) {
			// 0xFF Value indicates old Analog Gain Count of 16
			// Values < 0xFF indicate new gain count of 24 and new board rev
			// "DP5 G3 Configuration P" will not be used (==0xFF)
			if (m_DP5_Status->DPP_ECO < 0xFF) {		
				m_DP5_Status->isDP5_RevDxGains = true;
			}
			
		}

	}
}

string CDP5Status::DP5_Dx_OptionFlags(unsigned char DP5_Dx_Options) {
	unsigned char D7D6;
	unsigned char D5D4;
	unsigned char D3D0;
	string strRev("");
	stringex strfn;

	//D7-D6: 0 = DP5 Rev D
	//       1 = DP5 Rev E (future)
	//       2 = DP5 Rev F (future)
	//       3 = DP5 Rev G (future)
	D7D6 = ((DP5_Dx_Options >> 6) & 0x03) + 'D';
	//D5-D4: minor rev, 0-3 (i.e. Rev D0, D1 etc.)
	D5D4 = ((DP5_Dx_Options >> 4) & 0x03);
	//D3-D0: Configuration 0 = A, 1=B, 2=C... 15=P.
	D3D0 = (DP5_Dx_Options & 0x0F) + 'A';

	strRev = strfn.Format("DP5 Rev %c%d Configuration %c",D7D6,D5D4,D3D0);
	return(strRev);

}

string CDP5Status::ShowStatusValueStrings(DP4_FORMAT_STATUS m_DP5_Status) 
{ 
    string strConfig("");
    string strTemp;
	string strIntPart;
	string strFracPart;
	stringex strfn;

	strTemp = GetDeviceNameFromVal(m_DP5_Status.DEVICE_ID);
	strConfig = "Device Type: " + strTemp + "\r\n";
	strTemp = strfn.Format("Serial Number: %lu\r\n",m_DP5_Status.SerialNumber);	//SerialNumber
	strConfig += strTemp;
	strTemp = "Firmware: " + DppUtil.BYTEVersionToString(m_DP5_Status.Firmware);   
	strConfig += strTemp;
	if (m_DP5_Status.Firmware > 0x65) {
		strTemp = strfn.Format("  Build:  %d\r\n", m_DP5_Status.Build);
		strConfig += strTemp;
	} else {
		strConfig += "\r\n";
	}		
	strTemp = "FPGA: " + DppUtil.BYTEVersionToString(m_DP5_Status.FPGA) + "\r\n"; 
	strConfig += strTemp;
	if (m_DP5_Status.DEVICE_ID != dppMCA8000D) {
		strTemp = strfn.Format("Fast Count: %.0f\r\n",m_DP5_Status.FastCount);   	//FastCount
		strConfig += strTemp;
	}
	strTemp = strfn.Format("Slow Count: %.0f\r\n",m_DP5_Status.SlowCount);   	//SlowCount
	strConfig += strTemp;
	strTemp = strfn.Format("GP Count: %.0f\r\n",m_DP5_Status.GP_COUNTER);   	//GP Count
	strConfig += strTemp;

	if (m_DP5_Status.DEVICE_ID != dppMCA8000D) {
		strTemp = strfn.Format("Accumulation Time: %.0f\r\n",m_DP5_Status.AccumulationTime);	    //AccumulationTime
		strConfig += strTemp;
	}

	strTemp = strfn.Format("Real Time: %.0f\r\n",m_DP5_Status.RealTime);	    //RealTime
	strConfig += strTemp;

	if (m_DP5_Status.DEVICE_ID == dppMCA8000D) {
		strTemp = strfn.Format("Live Time: %.0f\r\n",m_DP5_Status.LiveTime);	    //RealTime
		strConfig += strTemp;
	}

	return strConfig;
}

string CDP5Status::PX5_OptionsString(DP4_FORMAT_STATUS m_DP5_Status)
{
	string strOptions("");
	string strValue("");

	if (m_DP5_Status.DEVICE_ID == dppPX5) {
        //m_DP5_Status.DPP_options = 1;
        //m_DP5_Status.HPGe_HV_INH = true;
        //m_DP5_Status.HPGe_HV_INH_POL = true;
        if (m_DP5_Status.DPP_options > 0) {
            //===============PX5 Options==================
            strOptions += "PX5 Options: ";
            if ((m_DP5_Status.DPP_options & 1) == 1) {
                strOptions += "HPGe HVPS\r\n";
            } else {
                strOptions += "Unknown\r\n";
            }
            //===============HPGe HVPS HV Status==================
            strOptions += "HPGe HV: ";
            if (m_DP5_Status.HPGe_HV_INH) {
                strOptions += "not inhibited\r\n";
            } else {
                strOptions += "inhibited\r\n";
            }
            //===============HPGe HVPS Inhibit Status==================
            strOptions += "INH Polarity: ";
            if (m_DP5_Status.HPGe_HV_INH_POL) {
                strOptions += "high\r\n";
            } else {
                strOptions += "low\r\n";
            }
        } else {
            strOptions += "PX5 Options: None\r\n";  //           strOptions += "No Options Installed"
        }
    }
	return strOptions; 
}

string CDP5Status::GetStatusValueStrings(DP4_FORMAT_STATUS m_DP5_Status) 
{ 
    string strConfig("");
    string strTemp;
	string strIntPart;
	string strFracPart;
	stringex strfn;

	strTemp = GetDeviceNameFromVal(m_DP5_Status.DEVICE_ID);
	strConfig = "Device Type: " + strTemp + "\r\n";
	strTemp = strfn.Format("Serial Number: %lu\r\n",m_DP5_Status.SerialNumber);	//SerialNumber
	strConfig += strTemp;
	strTemp = "Firmware: " + DppUtil.BYTEVersionToString(m_DP5_Status.Firmware);   
	strConfig += strTemp;
	if (m_DP5_Status.Firmware > 0x65) {
		strTemp = strfn.Format("  Build:  %d\r\n", m_DP5_Status.Build);
		strConfig += strTemp;
	} else {
		strConfig += "\r\n";
	}
	strTemp = "FPGA: " + DppUtil.BYTEVersionToString(m_DP5_Status.FPGA) + "\r\n"; 
	strConfig += strTemp;
	if (m_DP5_Status.DEVICE_ID != dppMCA8000D) {
		strTemp = strfn.Format("Fast Count: %.0f\r\n",m_DP5_Status.FastCount);   	//FastCount
		strConfig += strTemp;
	}
	strTemp = strfn.Format("Slow Count: %.0f\r\n",m_DP5_Status.SlowCount);   	//SlowCount
	strConfig += strTemp;
	strTemp = strfn.Format("GP Count: %.0f\r\n",m_DP5_Status.GP_COUNTER);   	//GP Count
	strConfig += strTemp;

	if (m_DP5_Status.DEVICE_ID != dppMCA8000D) {
		strTemp = strfn.Format("Accumulation Time: %.0f\r\n",m_DP5_Status.AccumulationTime);	    //AccumulationTime
		strConfig += strTemp;
	}

	strTemp = strfn.Format("Real Time: %.0f\r\n",m_DP5_Status.RealTime);	    //RealTime
	strConfig += strTemp;

	if (m_DP5_Status.DEVICE_ID == dppMCA8000D) {
		strTemp = strfn.Format("Live Time: %.0f\r\n",m_DP5_Status.LiveTime);	    //RealTime
		strConfig += strTemp;
	}

   if ((m_DP5_Status.DEVICE_ID != dppDP5G) && (m_DP5_Status.DEVICE_ID != dppTB5) && (m_DP5_Status.DEVICE_ID != dppMCA8000D)) {
        strTemp = strfn.Format("Detector Temp: %.0fK\r\n",m_DP5_Status.DET_TEMP);		//"##0°C") ' round to nearest degree
		strConfig += strTemp;
		strTemp = strfn.Format("Detector HV: %.0fV\r\n",m_DP5_Status.HV);
		strConfig += strTemp;
		strTemp = strfn.Format("Board Temp: %d°C\r\n",(int)m_DP5_Status.DP5_TEMP);
		strConfig += strTemp;
	} else if ((m_DP5_Status.DEVICE_ID == dppDP5G) || (m_DP5_Status.DEVICE_ID == dppTB5)) {		// GAMMARAD5,TB5
		if (m_DP5_Status.DET_TEMP > 0) {
			strTemp = strfn.Format("Detector Temp: %.1fK\r\n",m_DP5_Status.DET_TEMP);
			strConfig += strTemp;
		} else {
			strConfig += "";
		}
		strTemp = strfn.Format("HV Set: %.0fV\r\n",m_DP5_Status.HV);
		strConfig += strTemp;
	} else if (m_DP5_Status.DEVICE_ID == dppMCA8000D) {		// Digital MCA
		strTemp = strfn.Format("Board Temp: %d°C\r\n",(int)m_DP5_Status.DP5_TEMP);
		strConfig += strTemp;
	}
	if (m_DP5_Status.DEVICE_ID == dppPX5) {
		strTemp = PX5_OptionsString(m_DP5_Status);
		strConfig += strTemp;
		strTemp = strfn.Format("TEC V: %.3fV\r\n",m_DP5_Status.TEC_Voltage);
		strConfig += strTemp;
	}
	return strConfig;
}

void CDP5Status::Process_Diagnostics(Packet_In PIN, DiagDataType *dd, int device_type)
{
    long idxVal;
    string strVal;
	double DP5_ADC_Gain[10];  // convert each ADC count to engineering units - values calculated in FORM.LOAD
	double PC5_ADC_Gain[3];
	double PX5_ADC_Gain[12];
    stringex strfn;

    DP5_ADC_Gain[0] = 1.0 / 0.00286;                // 2.86mV/C
    DP5_ADC_Gain[1] = 1.0;                          // Vdd mon (out-of-scale)
    DP5_ADC_Gain[2] = (30.1 + 20.0) / 20.0;           // PWR
    DP5_ADC_Gain[3] = (13.0 + 20.0) / 20.0;            // 3.3V
    DP5_ADC_Gain[4] = (4.99 + 20.0) / 20.0;         // 2.5V
    DP5_ADC_Gain[5] = 1.0;                          // 1.2V
    DP5_ADC_Gain[6] = (35.7 + 20.0) / 20.0;          // 5.5V
    DP5_ADC_Gain[7] = (35.7 + 75.0) / 35.7;        // -5.5V (this one is tricky)
    DP5_ADC_Gain[8] = 1.0;                          // AN_IN
    DP5_ADC_Gain[9] = 1.0;                          // VREF_IN
    
    PC5_ADC_Gain[0] = 500.0;                        // HV: 1500V/3V
    PC5_ADC_Gain[1] = 100.0;                        // TEC: 300K/3V
    PC5_ADC_Gain[2] = (20.0 + 10.0) / 10.0;            // +8.5/5V

	//PX5_ADC_Gain[0] = (30.1 + 20.0) / 20.0;          // PWR
	PX5_ADC_Gain[0] = (69.8 + 20.0) / 20.0;          // 9V (was originally PWR)
    PX5_ADC_Gain[1] = (13.0 + 20.0) / 20.0;            // 3.3V
    PX5_ADC_Gain[2] = (4.99 + 20.0) / 20.0;          // 2.5V
    PX5_ADC_Gain[3] = 1.0;                         // 1.2V
    PX5_ADC_Gain[4] = (30.1 + 20.0) / 20.0;          // 5V
    PX5_ADC_Gain[5] = (10.7 + 75.0) / 10.7;         // -5V (this one is tricky)
    PX5_ADC_Gain[6] = (64.9 + 20.0) / 20.0;          // +PA
    PX5_ADC_Gain[7] = (10.7 + 75) / 10.7;        // -PA
    PX5_ADC_Gain[8] = (16.0 + 20.0) / 20.0;            // +TEC
    PX5_ADC_Gain[9] = 500.0;                       // HV: 1500V/3V
    PX5_ADC_Gain[10] = 100.0;                       // TEC: 300K/3V
    PX5_ADC_Gain[11] = 1.0 / 0.00286;               // 2.86mV/C

    dd->Firmware = PIN.DATA[0];
    dd->FPGA = PIN.DATA[1];
    strVal = "0x0" + FmtHex(PIN.DATA[2], 2) + FmtHex(PIN.DATA[3], 2) + FmtHex(PIN.DATA[4], 2);
    dd->SRAMTestData = strtol(strVal.c_str(),NULL,0);
    dd->SRAMTestPass = (dd->SRAMTestData == 0xFFFFFF);
    dd->TempOffset = PIN.DATA[180] + 256 * (PIN.DATA[180] > 127);  // 8-bit signed value

	if ((device_type == dppDP5) || (device_type == dppDP5X)) {
		for(idxVal=0;idxVal<10;idxVal++){
			dd->ADC_V[idxVal] = (float)((((PIN.DATA[5 + idxVal * 2] & 3) * 256) + PIN.DATA[6 + idxVal * 2]) * 2.44 / 1024.0 * DP5_ADC_Gain[idxVal]); // convert counts to engineering units (C or V)
		}
		dd->ADC_V[7] = dd->ADC_V[7] + dd->ADC_V[6] * (float)(1.0 - DP5_ADC_Gain[7]);  // -5.5V is a function of +5.5V
		dd->strTempRaw = strfn.Format("%   #.0f0C", dd->ADC_V[0] - 271.3);
		dd->strTempCal = strfn.Format("%   #.0f0C", (dd->ADC_V[0] - 280.0 + dd->TempOffset));
	} else if (device_type == dppPX5) {
        for(idxVal=0;idxVal<11;idxVal++){
            dd->ADC_V[idxVal] = (float)((((PIN.DATA[5 + idxVal * 2] & 15) * 256) + PIN.DATA[6 + idxVal * 2]) * 3.0 / 4096.0 * PX5_ADC_Gain[idxVal]);   // convert counts to engineering units (C or V)
		}
        dd->ADC_V[11] = (float)((((PIN.DATA[5 + 11 * 2] & 3) * 256) + PIN.DATA[6 + 11 * 2]) * 3.0 / 1024.0 * PX5_ADC_Gain[11]);  // convert counts to engineering units (C or V)
        dd->ADC_V[5] = (float)(dd->ADC_V[5] - (3.0 * PX5_ADC_Gain[5]) + 3.0); // -5V uses +3VR
        dd->ADC_V[7] = (float)(dd->ADC_V[7] - (3.0 * PX5_ADC_Gain[7]) + 3.0); // -PA uses +3VR
 		dd->strTempRaw = strfn.Format("%#.1fC", dd->ADC_V[11] - 271.3);
		dd->strTempCal = strfn.Format("%#.1fC", (dd->ADC_V[11] - 280.0 + dd->TempOffset));
	}	
	
    dd->PC5_PRESENT = false;  // assume no PC5, then check to see if there are any non-zero bytes
	for(idxVal=25;idxVal<=38;idxVal++) {
		if (PIN.DATA[idxVal] > 0) {
            dd->PC5_PRESENT = true;
            break;
        }
    }

    if (dd->PC5_PRESENT) {
		for(idxVal=0;idxVal<=2;idxVal++) {
			dd->PC5_V[idxVal] = (float)((((PIN.DATA[25 + idxVal * 2] & 15) * 256) + PIN.DATA[26 + idxVal * 2]) * 3.0 / 4096.0 * PC5_ADC_Gain[idxVal]); // convert counts to engineering units (C or V)
		}
		if (PIN.DATA[34] < 128) {
			dd->PC5_SN = (unsigned long)DppUtil.LongWordToDouble(31, PIN.DATA);
		} else {
			dd->PC5_SN = -1; // no PC5 S/N
		}
		if ((PIN.DATA[35] == 255) && (PIN.DATA[36] == 255)) {
			dd->PC5Initialized = false;
			dd->PC5DCAL = 0;
		} else {
			dd->PC5Initialized = true;
			dd->PC5DCAL = (float)(((float)(PIN.DATA[35]) * 256.0 + (float)(PIN.DATA[36])) * 3.0 / 4096.0);
		}
		dd->IsPosHV = ((PIN.DATA[37] & 128) == 128);
		dd->Is8_5VPreAmp = ((PIN.DATA[37] & 64) == 64);
		dd->Sup9VOn = ((PIN.DATA[38] & 8) == 8);
		dd->PreAmpON = ((PIN.DATA[38] & 4) == 4);
		dd->HVOn = ((PIN.DATA[38] & 2) == 2);
		dd->TECOn = ((PIN.DATA[38] & 1) == 1);
	} else {
        for(idxVal=0;idxVal<=2;idxVal++) {
            dd->PC5_V[idxVal] = 0;
		}
        dd->PC5_SN = -1; // no PC5 S/N
        dd->PC5Initialized = false;
        dd->PC5DCAL = 0;
        dd->IsPosHV = false;
        dd->Is8_5VPreAmp = false;
        dd->Sup9VOn = false;
        dd->PreAmpON = false;
        dd->HVOn = false;
        dd->TECOn = false;
    }
	for(idxVal=0;idxVal<=191;idxVal++) {
        dd->DiagData[idxVal] = PIN.DATA[idxVal + 39];
    }
	//string strData;
	//strData = DisplayBufferArray(PIN.DATA, 256);
	//SaveStringDataToFile(strData);
}

string CDP5Status::DiagnosticsToString(DiagDataType dd, int device_type)
{
    long idxVal;
	string strVal;
    string strDiag;
	stringex strfn;
    
    strDiag = "Firmware: " + VersionToStr(dd.Firmware) + "\r\n";
    strDiag += "FPGA: " + VersionToStr(dd.FPGA) + "\r\n";
    strDiag += "SRAM Test: ";
    if (dd.SRAMTestPass) {
        strDiag += "PASS\r\n";
    } else {
        strDiag += "ERROR @ 0x" + FmtHex(dd.SRAMTestData, 6) + "\r\n";
    }

	if ((device_type == dppDP5) || (device_type == dppDP5X)) {
		strDiag += "DP5 Temp (raw): " + dd.strTempRaw + "\r\n";
		strDiag += "DP5 Temp (cal'd): " + dd.strTempCal + "\r\n";
		strDiag += "PWR: " + FmtPc5Pwr(dd.ADC_V[2]) + "\r\n";
		strDiag += "3.3V: " + FmtPc5Pwr(dd.ADC_V[3]) + "\r\n";
		strDiag += "2.5V: " + FmtPc5Pwr(dd.ADC_V[4]) + "\r\n";
		strDiag += "1.2V: " + FmtPc5Pwr(dd.ADC_V[5]) + "\r\n";
		strDiag += "+5.5V: " + FmtPc5Pwr(dd.ADC_V[6]) + "\r\n";
		strDiag += "-5.5V: " + FmtPc5Pwr(dd.ADC_V[7]) + "\r\n";
		strDiag += "AN_IN: " + FmtPc5Pwr(dd.ADC_V[8]) + "\r\n";
		strDiag += "VREF_IN: " + FmtPc5Pwr(dd.ADC_V[9]) + "\r\n";

		strDiag += "\r\n";
		if (dd.PC5_PRESENT) {
			strDiag += "PC5: Present\r\n";
			strVal = strfn.Format("%dV",(int)(dd.PC5_V[0]));
			strDiag += "HV: " + strVal + "\r\n";
			strVal = strfn.Format("%#.1fK",dd.PC5_V[1]);
			strDiag += "Detector Temp: " + strVal + "\r\n";
			strDiag += "+8.5/5V: " + FmtPc5Pwr(dd.PC5_V[2]) + "\r\n";
			if (dd.PC5_SN > -1) {
				strDiag += "PC5 S/N: " + FmtLng(dd.PC5_SN) + "\r\n";
			} else {
				strDiag += "PC5 S/N: none\r\n";
			}
			if (dd.PC5Initialized) {
				strDiag += "PC5 DCAL: " + FmtPc5Pwr(dd.PC5DCAL) + "\r\n";
			} else {
				strDiag += "PC5 DCAL: Uninitialized\r\n";
			}
			strDiag += "PC5 Flavor: ";
			strDiag += IsAorB(dd.IsPosHV, "+HV, ", "-HV, ");
			strDiag += IsAorB(dd.Is8_5VPreAmp, "8.5V preamp", "5V preamp") + "\r\n";
			strDiag += "PC5 Supplies:\r\n";
			strDiag += "9V: " + OnOffStr(dd.Sup9VOn) + "\r\n";
			strDiag += "Preamp: " + OnOffStr(dd.PreAmpON) + "\r\n";
			strDiag += "HV: " + OnOffStr(dd.HVOn) + "\r\n";
			strDiag += "TEC: " + OnOffStr(dd.TECOn) + "\r\n";
		} else {
			strDiag += "PC5: Not Present\r\n";
		}
	} else if (device_type == dppPX5) {
		strDiag += "PX5 Temp (raw): " + dd.strTempRaw + "\r\n";
		strDiag += "PX5 Temp (cal'd): " + dd.strTempCal + "\r\n";
		//strDiag += "PWR: " + FmtPc5Pwr(dd.ADC_V[0]) + "\r\n";
		strDiag += "9V: " + FmtPc5Pwr(dd.ADC_V[0]) + "\r\n";
		strDiag += "3.3V: " + FmtPc5Pwr(dd.ADC_V[1]) + "\r\n";
		strDiag += "2.5V: " + FmtPc5Pwr(dd.ADC_V[2]) + "\r\n";
		strDiag += "1.2V: " + FmtPc5Pwr(dd.ADC_V[3]) + "\r\n";
		strDiag += "+5V: " + FmtPc5Pwr(dd.ADC_V[4]) + "\r\n";
		strDiag += "-5V: " + FmtPc5Pwr(dd.ADC_V[5]) + "\r\n";
		strDiag += "+PA: " + FmtPc5Pwr(dd.ADC_V[6]) + "\r\n";
		strDiag += "-PA: " + FmtPc5Pwr(dd.ADC_V[7]) + "\r\n";
		strDiag += "TEC: " + FmtPc5Pwr(dd.ADC_V[8]) + "\r\n";
		strDiag += "ABS(HV): " + FmtHvPwr(dd.ADC_V[9]) + "\r\n";
		strDiag += "DET_TEMP: " + FmtPc5Temp(dd.ADC_V[10]) + "\r\n";
	}

    strDiag += "\r\nDiagnostic Data\r\n";
    strDiag += "---------------\r\n";
	for(idxVal=0;idxVal<=191;idxVal++) {
        if ((idxVal % 8) == 0) { 
			strDiag += FmtHex(idxVal, 2) + ":";
		}
        strDiag += FmtHex(dd.DiagData[idxVal], 2) + " ";
        if ((idxVal % 8) == 7) { 
			strDiag += "\r\n";
		}
    }
    return (strDiag);
}

string CDP5Status::DiagStrPX5Option(DiagDataType dd, int device_type)
{
    long idxVal;
	string strVal;
    string strDiag;
 	stringex strfn;
   
    strDiag = "Firmware: " + VersionToStr(dd.Firmware) + "\r\n";
    strDiag += "FPGA: " + VersionToStr(dd.FPGA) + "\r\n";
    strDiag += "SRAM Test: ";
    if (dd.SRAMTestPass) {
        strDiag += "PASS\r\n";
    } else {
        strDiag += "ERROR @ 0x" + FmtHex(dd.SRAMTestData, 6) + "\r\n";
    }

	if (device_type == dppPX5) {
		strDiag += "PX5 Temp (raw): " + dd.strTempRaw + "\r\n";
		strDiag += "PX5 Temp (cal'd): " + dd.strTempCal + "\r\n";
		strDiag += "9V: " + FmtPc5Pwr(dd.ADC_V[0]) + "\r\n";
		strDiag += "3.3V: " + FmtPc5Pwr(dd.ADC_V[1]) + "\r\n";
		strDiag += "2.5V: " + FmtPc5Pwr(dd.ADC_V[2]) + "\r\n";
		strDiag += "TDET: " + FmtPc5Pwr(dd.ADC_V[3]) + "\r\n";
		strDiag += "+5V: " + FmtPc5Pwr(dd.ADC_V[4]) + "\r\n";
		strDiag += "-5V: " + FmtPc5Pwr(dd.ADC_V[5]) + "\r\n";
		strDiag += "+PA: " + FmtPc5Pwr(dd.ADC_V[6]) + "\r\n";
		strDiag += "-PA: " + FmtPc5Pwr(dd.ADC_V[7]) + "\r\n";
		strDiag += "TEC: " + FmtPc5Pwr(dd.ADC_V[8]) + "\r\n";
		strDiag += "ABS(HV): " + FmtHvPwr(dd.ADC_V[9]) + "\r\n";
		strDiag += "DET_TEMP: " + FmtPc5Temp(dd.ADC_V[10]) + "\r\n";
	}

    strDiag += "\r\nDiagnostic Data\r\n";
    strDiag += "---------------\r\n";
	for(idxVal=0;idxVal<=191;idxVal++) {
        if ((idxVal % 8) == 0) { 
			strDiag += FmtHex(idxVal, 2) + ":";
		}
        strDiag += FmtHex(dd.DiagData[idxVal], 2) + " ";
        if ((idxVal % 8) == 7) { 
			strDiag += "\r\n";
		}
    }
    return (strDiag);
}

string CDP5Status::FmtHvPwr(float fVal) 
{
	string strVal;
	stringex strfn;
	strVal = strfn.Format("%#.1fV", fVal);	// "#.##0V"
	return strVal;
}

string CDP5Status::FmtPc5Pwr(float fVal) 
{
	string strVal;
	stringex strfn;
	strVal = strfn.Format("%#.3fV", fVal);	// "#.##0V"
	return strVal;
}

string CDP5Status::FmtPc5Temp(float fVal) 
{
	string strVal;
	stringex strfn;
	strVal = strfn.Format("%#.1fK", fVal);	// "#.##0V"
	return strVal;
}

string CDP5Status::FmtHex(long FmtHex, long HexDig) 
{
	string strHex;
	string strFmt;
	stringex strfn;
	strFmt = strfn.Format("%d",HexDig);		// max size of 0 pad
	strFmt = "%0" + strFmt + "X";		// string format specifier
	strHex = strfn.Format(strFmt.c_str(), FmtHex);	// create padded string
	return strHex;
}

string CDP5Status::FmtLng(long lVal) 
{
	string strVal;
	stringex strfn;
	strVal = strfn.Format("%d", lVal);
	return strVal;
}

string CDP5Status::VersionToStr(unsigned char bVersion)
{
	string strVerMajor;
	string strVerMinor;
	string strVer;
	stringex strfn;
	strVerMajor = strfn.Format("%d",((bVersion & 0xF0) / 16));
	strVerMinor = strfn.Format("%02d",(bVersion & 0x0F));
	strVer = strVerMajor + "." + strVerMinor;
	return (strVer);
}

string CDP5Status::OnOffStr(bool bOn)
{
    if (bOn) {
        return("ON");
    } else {
        return("OFF");
    }
}

string CDP5Status::IsAorB(bool bIsA, string strA, string strB)
{
    if (bIsA) {
        return(strA);
    } else {
        return(strB);
    }
}

string CDP5Status::GetDeviceNameFromVal(int DeviceTypeVal) 
{
    string strDeviceType;
	switch(DeviceTypeVal) {
		case 0:
            strDeviceType = "DP5";
			break;
		case 1:
            strDeviceType = "PX5";
			break;
		case 2:
            strDeviceType = "DP5G";
			break;
		case 3:
            strDeviceType = "MCA8000D";
			break;
		case 4:
            strDeviceType = "TB5";
			break;
		case 5:
            strDeviceType = "DP5-X";
			break;
		default:           //if unknown set to DP5
            strDeviceType = "DP5";
			break;
	}
    return strDeviceType;
}

string CDP5Status::DisplayBufferArray(unsigned char buffer[], unsigned long bufSizeIn)
{
    unsigned long i;
	string strVal("");
	string strMsg("");
	stringex strfn;
	for(i=0;i<bufSizeIn;i++) {
		strVal = strfn.Format("%.2X ",buffer[i]);
		strMsg += strVal;
		//if (((i+1) % 16) == 0 ) { 
		//	strMsg += "\r\n";
		//} else 
		if (((i+1) % 8) == 0 ) {
		//	strMsg += "   ";
			//strMsg += "\r\n";
			strMsg += "\n";
		}
	}
	//strMsg += "\n";
	return strMsg;
}

void CDP5Status::SaveStringDataToFile(string strData)
{
	FILE  *out;
	string strFilename;
	string strError;
	stringex strfn;

	strFilename = "vcDP5_Data.txt";

	if ( (out = fopen(strFilename.c_str(),"w")) == (FILE *) NULL)
		strError = strfn.Format("Couldn't open %s for writing.\n", strFilename.c_str());
	else
	{
		fprintf(out,"%s\n",strData.c_str());
	}
	fclose(out);
}













