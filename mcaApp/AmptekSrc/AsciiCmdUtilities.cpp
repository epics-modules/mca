#include "AsciiCmdUtilities.h"

CAsciiCmdUtilities::CAsciiCmdUtilities(void)
{
}

CAsciiCmdUtilities::~CAsciiCmdUtilities(void)
{
}

string CAsciiCmdUtilities::MakeUpper(string StdString)
{
  const int length = (int)StdString.length();
  for(int i=0; i<length ; ++i)
  {
    StdString[i] = std::toupper(StdString[i]);
  }
  return StdString;
}

string CAsciiCmdUtilities::RemWhitespace(string strLine)
{
	unsigned int idxCh;
	string strCh;
	string strNoWSp;
	
	strNoWSp = "";
	if (strLine.find_first_of(Whitespace, 0) == std::string::npos) {		// string has no whitespace
		return strLine;
	} else {			// remove whitespace
		for (idxCh=0;idxCh<strLine.length();idxCh++) {
			strCh = strLine.substr(idxCh,1);
			if (strCh.find_first_of(Whitespace, 0) == std::string::npos) {		// char is not whitespace
				strNoWSp += strCh;
			}
		}
		return strNoWSp;
	}
}

//read and format [DP5 Configuration File]
string CAsciiCmdUtilities::GetDP5CfgStr(string strFilename)
{
	FILE *txtFile;
	char chLine[LINE_MAX];
	string strCfg;
	string strLine;
	long lPos;
	char ch;
	bool bCfgSection;
	int iCmpCfg;
	long lPosCfg;
	stringex strfn;

	bCfgSection = false;
	iCmpCfg = 1;
	if (( txtFile = fopen(strFilename.c_str(), "r")) == NULL) {  // Can't open input file
		return "";
	}
	strCfg = "";
	while((fgets(chLine, LINE_MAX, txtFile)) != NULL) {
		strLine = strfn.Format("%s",chLine);
		lPosCfg = (long)strLine.find('[');				// find a section (-1=not found,0=section found,>0=undefined)
		if (lPosCfg == 0) {								// Locate DP5 Configuration Section
			iCmpCfg = (long)strLine.find("[DP5 Configuration File]");
			if (iCmpCfg == 0) {							// Configuration Section Found
				bCfgSection = true;
			} else {									// Non-Configuration Section Found
				bCfgSection = false;
			}
		}
		if (bCfgSection) {								// Save Configuration Section Only
			strLine = MakeUpper(strLine);				// make all uppercase
			lPos = (long)strLine.find(';');				// find the delimiter (-1=not found,0=commented line)
			if (lPos > 0) {								// if has delimiter that is not first char
				strLine = strLine.substr(0,lPos + 1);	// remove string right of delimiter
				ch = strLine.at(0);					
				if ((ch >= 'A') && (ch <= 'Z')) {		// if is valid value
					strLine = RemWhitespace(strLine);	// remove whitespace in command sequence
					if (strLine.length() > 1) {			// check if command w/delimiter left
						strCfg += strLine;				// add to command string
					}
				}
			}
		}
		strLine = "";
	}
	fclose(txtFile);
	if (strCfg.length() > 0) {
		return strCfg;
	} else {
		return "";
	}
}

//read and format [DP5 SCA Configuration]
string CAsciiCmdUtilities::GetDP5ScaStr(string strFilename)
{
	FILE *txtFile;
	char chLine[LINE_MAX];
	string strCfg;
	string strLine;
	long lPos;
	char ch;
	bool bCfgSection;
	int iCmpCfg;
	long lPosCfg;
	stringex strfn;
	char chScaOld;
	char chSCA;
	int iScaBytes;
	string strScaIdx;
	string strScaCmd = "";

	bCfgSection = false;
	iCmpCfg = 1;
	if (( txtFile = fopen(strFilename.c_str(), "r")) == NULL) {  // Can't open input file
		return "";
	}
	chScaOld = '0';
	chSCA = '9';
	strCfg = "";
	while((fgets(chLine, LINE_MAX, txtFile)) != NULL) {
		strLine = strfn.Format("%s",chLine);
		lPosCfg = (long)strLine.find('[');				// find a section (-1=not found,0=section found,>0=undefined)
		if (lPosCfg == 0) {								// Locate DP5 Configuration Section
			iCmpCfg = (long)strLine.find("[DP5 SCA Configuration]");
			if (iCmpCfg == 0) {							// Configuration Section Found
				bCfgSection = true;
			} else {									// Non-Configuration Section Found
				bCfgSection = false;
			}
		}
		if (bCfgSection) {								// Save Configuration Section Only
			strLine = MakeUpper(strLine);				// make all uppercase
			lPos = (long)strLine.find(';');				// find the delimiter (-1=not found,0=commented line)
			if (lPos > 0) {								// if has delimiter that is not first char
				strLine = strLine.substr(0,lPos + 1);	// remove string right of delimiter
				ch = strLine.at(0);					
				if ((ch >= 'A') && (ch <= 'Z')) {		// if is valid value
					strLine = RemWhitespace(strLine);	// remove whitespace in command sequence
					if (strLine.length() > 1) {			// check if command w/delimiter left
						//strCfg += strLine;				// add to command string
						//strcat(strCfg,chLine);				// add to command string
						chSCA = strLine.at(4);				// determine SCA Index
						// remove index from SCA Command to create DPP ASCII Command format
						if ((chSCA > '0') && (chSCA < '9')) {
							if (chSCA != chScaOld) {		// Is same as current index?
								chScaOld = chSCA;			// if no, save current index
								// insert index command into string (SCAI=%c;)
								strScaIdx = strfn.Format("SCAI=%c;",chSCA);
								strCfg += strScaIdx;				// add sca index command to string
								strScaIdx = "";
							}
							strScaCmd = strLine.substr(0,4);
							for (iScaBytes=5;iScaBytes<=lPos;iScaBytes++) {	
								strScaCmd += strLine.at(iScaBytes);
							}
							strCfg += strScaCmd;				// add to command string
						} else {	// no index to remove, cmd was saved with SCAI command
							strCfg += strLine;					// add to command string
						}
					}
				}
			}
		}
		strLine = "";
	}
	fclose(txtFile);
	if (strCfg.length() > 0) {
		return strCfg;
	} else {
		return "";
	}
}

string CAsciiCmdUtilities::CreateResTestReadBackCmd(bool bSendCoarseFineGain, int DppType)
{
	string strCfg("");
    bool isINOF;

    isINOF = (DppType != dppDP5G) && (DppType != dppTB5);
	strCfg = "";
	strCfg += "CLCK=?;";
	strCfg += "TPEA=?;";
	if (bSendCoarseFineGain) { strCfg += "GAIF=?;"; }
	if (!bSendCoarseFineGain) { strCfg += "GAIN=?;"; }
	strCfg += "RESL=?;";
	strCfg += "TFLA=?;";
	strCfg += "TPFA=?;";
	strCfg += "PURE=?;";
	strCfg += "RTDE=?;";
	strCfg += "MCAS=?;";
	strCfg += "MCAC=?;";
	strCfg += "SOFF=?;";
	strCfg += "AINP=?;";
	if (isINOF) { strCfg += "INOF=?;"; }
	if (bSendCoarseFineGain) { strCfg += "GAIA=?;"; }
	return strCfg;
}

string CAsciiCmdUtilities::CreateFullReadBackCmd(bool PC5_PRESENT, int DppType, bool isDP5_RevDxGains, unsigned char DPP_ECO)
{
	string strCfg("");
    bool isHVSE;
    bool isPAPS;
    bool isTECS;
    bool isVOLU;
	bool isCON1;
	bool isCON2;
    bool isINOF;
    bool isBOOT;
	bool isGATE;
    bool isPAPZ;
	bool isSCTC;
	bool isDP5_DxK=false;
	bool isDP5_DxL=false;

	if (DppType == dppMCA8000D) {
		strCfg = CreateFullReadBackCmdMCA8000D(DppType);
		return strCfg;
	}

	// DP5 Rev Dx K,L needs PAPZ
	if ((DppType == dppDP5) && isDP5_RevDxGains) {
		if ((DPP_ECO & 0x0F) == 0x0A) {
			isDP5_DxK = true;
		}
		if ((DPP_ECO & 0x0F) == 0x0B) {
			isDP5_DxL = true;
		}
	}

    isHVSE = (((DppType != dppPX5) && PC5_PRESENT) || DppType == dppPX5);
    isPAPS = (DppType != dppDP5G) && (DppType != dppTB5);
    isTECS = (((DppType == dppDP5) && PC5_PRESENT) || (DppType == dppPX5) || (DppType == dppDP5X));
    isVOLU = (DppType == dppPX5);
    isCON1 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isCON2 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isINOF = (DppType != dppDP5G) && (DppType != dppTB5);
    isSCTC = (DppType == dppDP5G) || (DppType == dppTB5);
    isBOOT = ((DppType == dppDP5) || (DppType == dppDP5X));
	isGATE = ((DppType == dppDP5) || (DppType == dppDP5X));
	isPAPZ = (DppType == dppPX5) || isDP5_DxK || isDP5_DxL;

	strCfg = "";
	strCfg += "RESC=?;";
	strCfg += "CLCK=?;";
	strCfg += "TPEA=?;";
	strCfg += "GAIF=?;";
	strCfg += "GAIN=?;";
	strCfg += "RESL=?;";
	strCfg += "TFLA=?;";
	strCfg += "TPFA=?;";
	strCfg += "PURE=?;";
	if (isSCTC) { strCfg += "SCTC=?;"; }
	strCfg += "RTDE=?;";
	strCfg += "MCAS=?;";
	strCfg += "MCAC=?;";
	strCfg += "SOFF=?;";
	strCfg += "AINP=?;";
	if (isINOF) { strCfg += "INOF=?;"; }
	strCfg += "GAIA=?;";
	strCfg += "CUSP=?;";
	strCfg += "PDMD=?;";
	strCfg += "THSL=?;";
	strCfg += "TLLD=?;";
	strCfg += "THFA=?;";
	strCfg += "DACO=?;";
	strCfg += "DACF=?;";
	strCfg += "RTDS=?;";
	strCfg += "RTDT=?;";
	strCfg += "BLRM=?;";
	strCfg += "BLRD=?;";
	strCfg += "BLRU=?;";
	if (isGATE) { strCfg += "GATE=?;"; }
	strCfg += "AUO1=?;";
	strCfg += "PRET=?;";
	strCfg += "PRER=?;";
	strCfg += "PREC=?;";
	strCfg += "PRCL=?;";
	strCfg += "PRCH=?;";
	if (isHVSE) { strCfg += "HVSE=?;"; }
	if (isTECS) { strCfg += "TECS=?;"; }
	if (isPAPZ) { strCfg += "PAPZ=?;"; }
	if (isPAPS) { strCfg += "PAPS=?;"; }
	strCfg += "SCOE=?;";
	strCfg += "SCOT=?;";
	strCfg += "SCOG=?;";
	strCfg += "MCSL=?;";
	strCfg += "MCSH=?;";
	strCfg += "MCST=?;";
	strCfg += "AUO2=?;";
	strCfg += "TPMO=?;";
	strCfg += "GPED=?;";
	strCfg += "GPIN=?;";
	strCfg += "GPME=?;";
	strCfg += "GPGA=?;";
	strCfg += "GPMC=?;";
	strCfg += "MCAE=?;";
	if (isVOLU) { strCfg += "VOLU=?;"; }
	if (isCON1) { strCfg += "CON1=?;"; }
	if (isCON2) { strCfg += "CON2=?;"; }
	if (isBOOT) { strCfg += "BOOT=?;"; }
	return strCfg;
}

string CAsciiCmdUtilities::CreateFullReadBackCmdMCA8000D(int DppType)
{
	string strCfg("");

	if (DppType == dppMCA8000D) {
		strCfg += "RESC=?;";
		strCfg += "PURE=?;";
		strCfg += "MCAS=?;";
		strCfg += "MCAC=?;";
		strCfg += "SOFF=?;";
		strCfg += "GAIA=?;";
		strCfg += "PDMD=?;";
		strCfg += "THSL=?;";
		strCfg += "TLLD=?;";
		strCfg += "GATE=?;";
		strCfg += "AUO1=?;";
		//strCfg += "PRET=?;";
		strCfg += "PRER=?;";
		strCfg += "PREL=?;";
		strCfg += "PREC=?;";
		strCfg += "PRCL=?;";
		strCfg += "PRCH=?;";
		strCfg += "SCOE=?;";
		strCfg += "SCOT=?;";
		strCfg += "SCOG=?;";
		strCfg += "MCSL=?;";
		strCfg += "MCSH=?;";
		strCfg += "MCST=?;";
		strCfg += "AUO2=?;";
		strCfg += "GPED=?;";
		strCfg += "GPIN=?;";
		strCfg += "GPME=?;";
		strCfg += "GPGA=?;";
		strCfg += "GPMC=?;";
		strCfg += "MCAE=?;";
		//strCfg += "VOLU=?;";
		//strCfg += "CON1=?;";
		//strCfg += "CON2=?;";
		strCfg += "PDMD=?;";
	}
	return strCfg;
}

string CAsciiCmdUtilities::RemoveCmd(string strCmd, string strCfgData)
{
	int iStart,iEnd,iCmd;
	string strNew;

	strNew = "";
	if (strCfgData.length() < 7) { return strCfgData; }	// no data
	if (strCmd.length() != 4) {	return strCfgData; }		// bad command
	iCmd = (int)strCfgData.find(strCmd+"=",0);
	if (iCmd == -1) { return strCfgData; }						// cmd not found
	iStart = iCmd;
	iEnd = (int)strCfgData.find(";",iCmd);
	if (iEnd == -1) { return strCfgData; }						// end not found
	if (iEnd <= iStart) { return strCfgData; }					// unknown error
	strNew = strCfgData.substr(0,iStart) + strCfgData.substr(iEnd+1);
	return strNew;
}

////removes selected command by dpp device type
string CAsciiCmdUtilities::RemoveCmdByDeviceType(string strCfgDataIn, bool PC5_PRESENT, int DppType, bool isDP5_RevDxGains, unsigned char DPP_ECO)
{
	string strCfgData;
    bool isHVSE;
    bool isPAPS;
    bool isTECS;
    bool isVOLU;
	bool isCON1;
	bool isCON2;
    bool isINOF;
    bool isBOOT;
	bool isGATE;
	bool isPAPZ;
	bool isSCTC;
	bool isPREL;
	bool isDP5_DxK=false;
	bool isDP5_DxL=false;

	strCfgData = strCfgDataIn;
	if (DppType == dppMCA8000D) {
		strCfgData = Remove_MCA8000D_Cmds(strCfgData,DppType);
		return strCfgData;
	}

	// DP5 Rev Dx K,L needs PAPZ
	if ((DppType == dppDP5) && isDP5_RevDxGains) {
		if ((DPP_ECO & 0x0F) == 0x0A) {
			isDP5_DxK = true;
		}
		if ((DPP_ECO & 0x0F) == 0x0B) {
			isDP5_DxL = true;
		}
	}

    isHVSE = (((DppType != dppPX5) && PC5_PRESENT) || DppType == dppPX5);
    isPAPS = (DppType != dppDP5G) && (DppType != dppTB5);
    isTECS = (((DppType == dppDP5) && PC5_PRESENT) || (DppType == dppPX5) || (DppType == dppDP5X));
    isVOLU = (DppType == dppPX5);
    isCON1 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isCON2 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isINOF = (DppType != dppDP5G) && (DppType != dppTB5);
	isSCTC = (DppType == dppDP5G) || (DppType == dppTB5);
    isBOOT = ((DppType == dppDP5) || (DppType == dppDP5X));
	isGATE = ((DppType == dppDP5) || (DppType == dppDP5X));
	isPAPZ = (DppType == dppPX5) || isDP5_DxK || isDP5_DxL;
	isPREL = (DppType == dppMCA8000D);
	if (!isHVSE) { strCfgData = RemoveCmd("HVSE", strCfgData); }  //High Voltage Bias
	if (!isPAPS) { strCfgData = RemoveCmd("PAPS", strCfgData); }  //Preamp Voltage
	if (!isTECS) { strCfgData = RemoveCmd("TECS", strCfgData); }  //Cooler Temperature
	if (!isVOLU) { strCfgData = RemoveCmd("VOLU", strCfgData); }  //px5 speaker
	if (!isCON1) { strCfgData = RemoveCmd("CON1", strCfgData); }  //connector 1
	if (!isCON2) { strCfgData = RemoveCmd("CON2", strCfgData); }  //connector 2
	if (!isINOF) { strCfgData = RemoveCmd("INOF", strCfgData); }  //input offset
	if (!isBOOT) { strCfgData = RemoveCmd("BOOT", strCfgData); }  //PC5 On At StartUp
	if (!isGATE) { strCfgData = RemoveCmd("GATE", strCfgData); }  //Gate input
	if (!isPAPZ) { strCfgData = RemoveCmd("PAPZ", strCfgData); }  //Pole-Zero
	if (!isSCTC) { strCfgData = RemoveCmd("SCTC", strCfgData); }  //Scintillator Time Constant
	if (!isPREL) { strCfgData = RemoveCmd("PREL", strCfgData); }  //Preset Live Time
	return strCfgData;
}

////removes selected command by dpp device type
string CAsciiCmdUtilities::RemoveCmdByDeviceTypeDP5DxK(string strCfgDataIn, bool PC5_PRESENT, int DppType)
{
	string strCfgData;
    bool isHVSE;
    bool isPAPS;
    bool isTECS;
    bool isVOLU;
	bool isCON1;
	bool isCON2;
    bool isINOF;
    bool isBOOT;
	bool isGATE;
	bool isPAPZ;
	bool isSCTC;
	bool isPREL;

	strCfgData = strCfgDataIn;
	//if (DppType == dppMCA8000D) {
	//	strCfgData = Remove_MCA8000D_Cmds(strCfgData,DppType);
	//	return strCfgData;
	//}
    isHVSE = (((DppType != dppPX5) && PC5_PRESENT) || DppType == dppPX5);
    isPAPS = (DppType != dppDP5G) && (DppType != dppTB5);
    isTECS = (((DppType == dppDP5) && PC5_PRESENT) || (DppType == dppPX5) || (DppType == dppDP5X));
    isVOLU = (DppType == dppPX5);
    isCON1 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isCON2 = ((DppType != dppDP5) && (DppType != dppDP5X));
    isINOF = (DppType != dppDP5G) && (DppType != dppTB5);
	isSCTC = (DppType == dppDP5G) || (DppType == dppTB5);
    isBOOT = ((DppType == dppDP5) || (DppType == dppDP5X));
	isGATE = ((DppType == dppDP5) || (DppType == dppDP5X));
	//isPAPZ = (DppType == dppPX5);		// DP5 Rev Dx K,L needs PAPZ
	isPAPZ = true;							// DP5 Rev Dx K,L needs PAPZ
	isPREL = (DppType == dppMCA8000D);
	if (!isHVSE) { strCfgData = RemoveCmd("HVSE", strCfgData); }  //High Voltage Bias
	if (!isPAPS) { strCfgData = RemoveCmd("PAPS", strCfgData); }  //Preamp Voltage
	if (!isTECS) { strCfgData = RemoveCmd("TECS", strCfgData); }  //Cooler Temperature
	if (!isVOLU) { strCfgData = RemoveCmd("VOLU", strCfgData); }  //px5 speaker
	if (!isCON1) { strCfgData = RemoveCmd("CON1", strCfgData); }  //connector 1
	if (!isCON2) { strCfgData = RemoveCmd("CON2", strCfgData); }  //connector 2
	if (!isINOF) { strCfgData = RemoveCmd("INOF", strCfgData); }  //input offset
	if (!isBOOT) { strCfgData = RemoveCmd("BOOT", strCfgData); }  //PC5 On At StartUp
	if (!isGATE) { strCfgData = RemoveCmd("GATE", strCfgData); }  //Gate input
	if (!isPAPZ) { strCfgData = RemoveCmd("PAPZ", strCfgData); }  //Pole-Zero
	if (!isSCTC) { strCfgData = RemoveCmd("SCTC", strCfgData); }  //Scintillator Time Constant
	if (!isPREL) { strCfgData = RemoveCmd("PREL", strCfgData); }  //Preset Live Time
	return strCfgData;
}

////removes MCA8000D commands
string CAsciiCmdUtilities::Remove_MCA8000D_Cmds(string strCfgDataIn, int DppType)
{
	string strCfgData;
	
	strCfgData = strCfgDataIn;
	if (DppType == dppMCA8000D) {
		strCfgData = RemoveCmd("CLCK", strCfgData);
		strCfgData = RemoveCmd("TPEA", strCfgData);
		strCfgData = RemoveCmd("GAIF", strCfgData);
		strCfgData = RemoveCmd("GAIN", strCfgData);
		strCfgData = RemoveCmd("RESL", strCfgData);
		strCfgData = RemoveCmd("TFLA", strCfgData);
		strCfgData = RemoveCmd("TPFA", strCfgData);
		//strCfgData = RemoveCmd("PURE", strCfgData);
		strCfgData = RemoveCmd("RTDE", strCfgData);
		strCfgData = RemoveCmd("AINP", strCfgData);
		strCfgData = RemoveCmd("INOF", strCfgData);
		strCfgData = RemoveCmd("CUSP", strCfgData);
		strCfgData = RemoveCmd("THFA", strCfgData);
		strCfgData = RemoveCmd("DACO", strCfgData);
		strCfgData = RemoveCmd("DACF", strCfgData);
		strCfgData = RemoveCmd("RTDS", strCfgData);
		strCfgData = RemoveCmd("RTDT", strCfgData);
		strCfgData = RemoveCmd("BLRM", strCfgData);
		strCfgData = RemoveCmd("BLRD", strCfgData);
		strCfgData = RemoveCmd("BLRU", strCfgData);
		strCfgData = RemoveCmd("PRET", strCfgData);
		strCfgData = RemoveCmd("HVSE", strCfgData);
		strCfgData = RemoveCmd("TECS", strCfgData);
		strCfgData = RemoveCmd("PAPZ", strCfgData);
		strCfgData = RemoveCmd("PAPS", strCfgData);
		strCfgData = RemoveCmd("TPMO", strCfgData);
		strCfgData = RemoveCmd("SCAH", strCfgData);
		strCfgData = RemoveCmd("SCAI", strCfgData);
		strCfgData = RemoveCmd("SCAL", strCfgData);
		strCfgData = RemoveCmd("SCAO", strCfgData);
		strCfgData = RemoveCmd("SCAW", strCfgData);
		strCfgData = RemoveCmd("BOOT", strCfgData);

		// added to list late, recheck at later date 20120817
		strCfgData = RemoveCmd("CON1", strCfgData);
		strCfgData = RemoveCmd("CON2", strCfgData);

		// not implemented as of 20120817, will be implemented at some time
		strCfgData = RemoveCmd("VOLU", strCfgData);	 
	}
	return strCfgData;
}

// replaces all occurrences of substring in string
std::string CAsciiCmdUtilities::ReplaceCmdText(std::string strInTextIn, std::string strFrom, std::string strTo)
{
	std::string strReplaceText;
	std::string strInText; 
	std::string strOutText; 
	int lFromLen; 
	int lMatchPos; 

	strInText = strInTextIn;
	strOutText = "";
	lFromLen = (int)strFrom.length();
	while (strInText.length()>0) {
		lMatchPos = (1+(int)strInText.find(strFrom));
		if (lMatchPos==0) {
			strOutText = strOutText+strInText;
			strInText = "";
		} else {
			strOutText = strOutText+strInText.substr(0,lMatchPos-1)+strTo;
			strInText = std::string(strInText).substr(lMatchPos+lFromLen-1);
		}
	}
	strReplaceText = strOutText;
	return strReplaceText;
}

// breaks ASCII Command string into two chuncks, returns split position
int CAsciiCmdUtilities::GetCmdChunk(std::string strCmd)
{
	int GetCmdChunk = 0;
	int idxCfg; 
	int lChunk; 
	int lEnd; 
	GetCmdChunk = 0;
	lChunk = 0;
	lEnd = 0;
	for(idxCfg=1; idxCfg<=(int)strCmd.length();idxCfg++) {
		lChunk = (int)(1+strCmd.find(";", lEnd));
		if ((lChunk==0) || (lChunk>512)) {
			break;
		}
		lEnd = lChunk;
	}
	GetCmdChunk = lEnd;
	return GetCmdChunk;
}

bool CAsciiCmdUtilities::CopyAsciiData(unsigned char Data[], string strCfg, long lLen)
{
	long idxData;
	const char *c_str1 = strCfg.c_str();

	if (lLen > 0) {
		for(idxData=0;idxData<lLen;idxData++) {
			Data[idxData] = c_str1[idxData];
		}
		return true;
	} else {
		return false;
	}
	return false;
}
