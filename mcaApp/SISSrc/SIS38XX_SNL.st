program SIS38XX_SNL("P=SIS:3820, MCA=mca, numMCAs=2")

/*  This sequencer program works with SIS3820.template 
 *  It supports multi-element MCA operations
 *
 *  Mark Rivers
 *  April 19, 2011
 *
 * This program must be compiled with the recursive option so that multiple copies
 * can run at once */
option +r;

/* Allow the program to start, even if not all assigned channels are connected.
 * This could be useful if the SNL is running on a different ioc. */
option -c;

%%#include <stdlib.h>
%%#include <errlog.h>
%%#include <string.h>

/* Maximum number of detectors supported */
#define MAX_MCAS 32

int i;
int n;
int numMCAs;
char temp[100];

char *prefix;
char *dxp;
char *mca;

int     ReadMCA[MAX_MCAS]; 
assign  ReadMCA to
          {"","","","","","","","","","","","","","","","",
           "","","","","","","","","","","","","","","",""};
int     ReadMCAs; assign ReadMCAs to "{P}DoReadAll";
monitor ReadMCAs; evflag ReadMCAsMon; sync ReadMCAs ReadMCAsMon;

int     statusMCA[MAX_MCAS];
assign  statusMCA to
          {"","","","","","","","","","","","","","","","",
           "","","","","","","","","","","","","","","",""};
int     StatusMCAs; assign StatusMCAs to "{P}DoStatusAll";
monitor StatusMCAs; evflag StatusMCAsMon; sync StatusMCAs StatusMCAsMon;

int     HardwareAcquiring; assign HardwareAcquiring to "{P}HardwareAcquiring";
monitor HardwareAcquiring; evflag HardwareAcquiringMon; sync HardwareAcquiring HardwareAcquiringMon;

int     SNL_Connected;  assign SNL_Connected      to "{P}SNL_Connected";
int     AsynDebug;      assign AsynDebug          to "{P}Asyn.TB1";
monitor AsynDebug;

int Acquiring; assign Acquiring to "{P}Acquiring";

ss mca_control {
    state init {
        when() {
            prefix = macValueGet("P");
            mca = macValueGet("MCA");
            numMCAs = atoi(macValueGet("NUM_MCAS"));
            if ((numMCAs <= 0) || (numMCAs > MAX_MCAS)) {
                printf ("NUM_MCAS is illegal.\n");
                numMCAs = 0;
            }
            for (i=0; i<numMCAs; i++) {
                n = i+1;
                sprintf(temp, "%s%s%d.READ", prefix, mca, n);
                pvAssign(ReadMCA[i], temp);

                sprintf(temp, "%s%s%d.PROC", prefix, mca, n);
                pvAssign(statusMCA[i], temp);
           }
        } state waitConnected
    }
    state waitConnected {

        when (numMCAs <= 0) {
          printf (">>>>>>>>>>>>>> SIS is dead. <<<<<<<<<<<<<<\n");
        } state dead

        when (pvAssignCount () == pvConnectCount ()) {
          printf ("SIS: All channels connected.\n");
          SNL_Connected = 1;
          pvPut(SNL_Connected);
        } state monitor_changes

    }
    state dead {
      when (delay (3600.0)) {
      } state dead
    }

    state monitor_changes {
        when (pvAssignCount () != pvConnectCount ()) {
          printf ("SIS: Not all channels connected.\n");
        } state waitConnected

        when(efTestAndClear(ReadMCAsMon) && (ReadMCAs == 1)) {
            if (AsynDebug) printf("SIS.st: Read MCA data\n");
            for (i=0; i<numMCAs; i++) {
                ReadMCA[i] = 1;
                pvPut(ReadMCA[i]);
            }
            ReadMCAs = 0;
            pvPut(ReadMCAs);
        } state monitor_changes

        when(efTestAndClear(StatusMCAsMon) && (StatusMCAs == 1)) {
            if (AsynDebug) printf("SIS.st: Read MCA status\n");
            for (i=0; i<numMCAs; i++) {
                statusMCA[i] = 1;
                pvPut(statusMCA[i]);
            }
            StatusMCAs = 0;
            pvPut(StatusMCAs);
        } state monitor_changes

        when(efTestAndClear(HardwareAcquiringMon)) {
            if (AsynDebug) printf("SIS.st: HardwareAcquiringMon, HardwareAcquiring=%d\n", HardwareAcquiring);
            /* If the detector is acquiring then force each MCA record to read status */
            if (!HardwareAcquiring) {
                /* If the detector is done then force each MCA record to read data */
                for (i=0; i<numMCAs; i++) {
                    ReadMCA[i] = 1;
                    pvPut(ReadMCA[i], SYNC);
                }
                /* Clear the Acquiring busy record */
                Acquiring = 0;
                pvPut(Acquiring);
            }
        } state monitor_changes
    }
}

