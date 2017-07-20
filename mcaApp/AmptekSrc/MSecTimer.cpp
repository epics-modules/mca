#include ".\msectimer.h"

CMSecTimer::CMSecTimer(void)
{
}

CMSecTimer::~CMSecTimer(void)
{
}

DWORD CMSecTimer::msTimeDiff(DWORD msStartTime, DWORD msTestTime)
{
	DWORD64 RollOver = (DWORD)pow(2.0,32);
	DWORD64 msStartTime64;
	DWORD64 msTestTime64;
	DWORD64 msTimeDiff64;
	DWORD msTimeDiff32;
	if (msStartTime > msTestTime) {	// rollover
		msTestTime64 = (DWORD64)msTestTime + RollOver;
		msStartTime64 = (DWORD64)msStartTime;
		msTimeDiff64 = msTestTime64 - msTestTime64;
		if (msTimeDiff64 < RollOver) {
			msTimeDiff32 = (DWORD)(msTimeDiff64);
		} else {
			msTimeDiff32 = 0;
		}
	} else {
		msTimeDiff32 = msTestTime - msStartTime;
	}
	return (msTimeDiff32);
}

BOOL CMSecTimer::msTimeExpired(DWORD msStartTime, DWORD msDelay)
{
	BOOL isExpired;
	DWORD msTestTime;
	DWORD64 RollOver = (DWORD)pow(2.0,32);
	DWORD64 msStartTime64;
	DWORD64 msTestTime64;
	DWORD64 msTimeDiff64;
	DWORD msTimeDiff32;

	msTestTime = timeGetTime();
	if (msStartTime > msTestTime) {	// rollover
		msTestTime64 = (DWORD64)msTestTime + RollOver;
		msStartTime64 = (DWORD64)msStartTime;
		msTimeDiff64 = msTestTime64 - msTestTime64;
		if (msTimeDiff64 < RollOver) {
			msTimeDiff32 = (DWORD)(msTimeDiff64);
		} else {
			msTimeDiff32 = 0;
		}
	} else {
		msTimeDiff32 = msTestTime - msStartTime;
	}
	isExpired = (msTimeDiff32 >= msDelay);
	return (isExpired);
}

BOOL CMSecTimer::msTimer(DWORD msDelay)
{
	MSG msg;
	DWORD msStartTime;
	DWORD msTestTime;
	BOOL isExpired = FALSE;
	DWORD msTimeDiff32 = 0;
	msStartTime = timeGetTime();
	msTestTime = timeGetTime();
	do {
		isExpired = msTimeExpired(msStartTime, msDelay);
		msTimeDiff32 = msTimeDiff(msStartTime, msTestTime);
		if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) 
		{
	    	TranslateMessage(&msg);
	    	DispatchMessage(&msg);
		}
	} while (!isExpired && (msTimeDiff32 <= msDelay));
	return (TRUE);
}
