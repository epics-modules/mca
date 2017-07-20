#pragma once
#include <math.h>
#include <Mmsystem.h>

class CMSecTimer
{
public:
	CMSecTimer(void);
	~CMSecTimer(void);
	DWORD msTimeDiff(DWORD msStartTime, DWORD msTestTime);
	BOOL msTimeExpired(DWORD msStartTime, DWORD msDelay);
	BOOL msTimer(DWORD msDelay);
};
