//
// Created by admin on 2019-08-26.
//
#include "separteExterns.h"

void printQOSData(int exitCode);

extern Boolean waitForResponseToTEARDOWN;

void continueAfterTEARDOWN(RTSPClient* client, int resultCode, char* resultString);
void tearDownSession(MediaSession* session, RTSPClient::responseHandler* afterFunc);

Boolean areAlreadyShuttingDown = False;
int shutdownExitCode;
void shutdown(int exitCode) {
    if (areAlreadyShuttingDown) return; // in case we're called after receiving a RTCP "BYE" while in the middle of a "TEARDOWN".
    areAlreadyShuttingDown = True;

    shutdownExitCode = exitCode;
    if (env != NULL) {
        env->taskScheduler().unscheduleDelayedTask(periodicFileOutputTask);
        env->taskScheduler().unscheduleDelayedTask(sessionTimerTask);
        env->taskScheduler().unscheduleDelayedTask(sessionTimeoutBrokenServerTask);
        env->taskScheduler().unscheduleDelayedTask(arrivalCheckTimerTask);
        env->taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
        env->taskScheduler().unscheduleDelayedTask(qosMeasurementTimerTask);
    }

    if (qosMeasurementIntervalMS > 0) {
        printQOSData(exitCode);
    }

    // Teardown, then shutdown, any outstanding RTP/RTCP subsessions
    Boolean shutdownImmediately = True; // by default
    if (session != NULL) {
        RTSPClient::responseHandler* responseHandlerForTEARDOWN = NULL; // unless:
        if (waitForResponseToTEARDOWN) {
            shutdownImmediately = False;
            responseHandlerForTEARDOWN = continueAfterTEARDOWN;
        }
        tearDownSession(session, responseHandlerForTEARDOWN);
    }

    if (shutdownImmediately) continueAfterTEARDOWN(NULL, 0, NULL);
}