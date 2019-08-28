//
// Created by admin on 2019-08-26.
//

#pragma once
#include <liveMedia.hh>
#include <string>
// 추가한 extern
extern UsageEnvironment* env;
extern unsigned short desiredPortNum;
extern char const* singleMedium;
extern unsigned interPacketGapMaxTime;
extern Boolean playContinuously;
extern int simpleRTPoffsetArg;
extern Boolean oneFilePerFrame;
extern Boolean notifyOnPacketArrival;
extern Boolean sendOptionsRequest;


extern Boolean streamUsingTCP;
extern portNumBits tunnelOverHTTPPortNum;

extern Authenticator* ourAuthenticator;
extern char* usernameForREGISTER;
extern char* passwordForREGISTER;
extern UserAuthenticationDatabase* authDBForREGISTER;
extern Boolean sendKeepAlivesToBrokenServers;
extern std::string fileNamePrefix;
extern unsigned fileSinkBufferSize;
extern unsigned socketInputBufferSize;

extern Boolean syncStreams;

extern unsigned qosMeasurementIntervalMS; // 0 means: Don't output QOS data
extern float scale;

extern Boolean forceMulticastOnUnspecified;

extern TaskToken periodicFileOutputTask;
extern TaskToken sessionTimerTask;
extern TaskToken sessionTimeoutBrokenServerTask;
extern TaskToken arrivalCheckTimerTask;
extern TaskToken interPacketGapCheckTimerTask;
extern TaskToken qosMeasurementTimerTask;

extern MediaSession* session;
extern std::string streamURL;
