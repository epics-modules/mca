//Ip330Sweep.h

/*
    Author: Mark Rivers
    Date: 10/27/99
  
    04/20/00  MLR  Added private members for timing
    04/09/03  MLR  Converted Ip330Sweep.h to this base class, fastSweep.h

*/

#ifndef fastSweepH
#define fastSweepH

class fastSweep {
public:
    fastSweep(int firstChan, int lastChan, int maxPoints);
    virtual void startAcquire();
    virtual void stopAcquire();
    virtual void erase();
    virtual double setMicroSecondsPerPoint(double microSeconds)=0;
    virtual double getMicroSecondsPerPoint()=0;
    virtual void nextPoint(int *newData);
    virtual int setNumPoints(int numPoints);
    virtual int getNumPoints();
    virtual int setRealTime(double time);
    virtual int setLiveTime(double time);
    virtual double getElapsedTime();
    virtual int getStatus();
    virtual void getData(int channel, int *data);
    int firstChan;
    int lastChan;
private:
    int maxPoints;
    int acquiring;
    int numAcquired;
    int realTime;
    int liveTime;
    int elapsedTime;
    int startTime;
    int numChans;
    int numPoints;
    int *pData;
};

class fastSweepServer {
public:
    fastSweepServer(const char *serverName, fastSweep *pfastSweep, 
                    int queueSize);
    static void fastServer(fastSweepServer *);
    fastSweep *pFastSweep;
    MessageServer *pMessageServer;
};

#endif //fastSweepH
