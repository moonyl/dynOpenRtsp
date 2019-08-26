//
// Created by admin on 2019-08-26.
//

#pragma once
#include <liveMedia.hh>
// 추가한 extern
extern UsageEnvironment* env;
extern unsigned short desiredPortNum;
extern Boolean generateMP4Format;
extern Boolean outputAVIFile;
extern Boolean audioOnly;
extern Boolean videoOnly;
extern char const* singleMedium;
extern int verbosityLevel; // by default, print verbose output
extern double duration;
extern double durationSlop; // extra seconds to play at the end
extern unsigned interPacketGapMaxTime;
extern Boolean playContinuously;
extern int simpleRTPoffsetArg;
extern Boolean oneFilePerFrame;
extern Boolean notifyOnPacketArrival;
extern Boolean sendOptionsRequest;
extern Boolean sendOptionsRequestOnly;

extern Boolean controlConnectionUsesTCP;

extern Boolean streamUsingTCP;
extern portNumBits tunnelOverHTTPPortNum;
extern char* username;
extern char* password;
extern char* proxyServerName;
extern unsigned short proxyServerPortNum;
extern Boolean allowProxyServers;
extern Authenticator* ourAuthenticator;
extern char* usernameForREGISTER;
extern char* passwordForREGISTER;
extern UserAuthenticationDatabase* authDBForREGISTER;
extern Boolean sendKeepAlivesToBrokenServers;
extern unsigned char desiredAudioRTPPayloadFormat;
extern char* mimeSubtype;
extern unsigned short movieWidth; // default
extern Boolean movieWidthOptionSet;
extern unsigned short movieHeight; // default
extern Boolean movieHeightOptionSet;
extern unsigned movieFPS; // default
extern Boolean movieFPSOptionSet;
extern char const* fileNamePrefix;
extern unsigned fileSinkBufferSize;
extern unsigned socketInputBufferSize;
extern Boolean packetLossCompensate;
extern Boolean syncStreams;
extern Boolean generateHintTracks;
extern char* userAgent;
extern unsigned qosMeasurementIntervalMS; // 0 means: Don't output QOS data
extern double initialSeekTime;
extern char* initialAbsoluteSeekTime;
extern char* initialAbsoluteSeekEndTime;
extern float scale;
extern portNumBits handlerServerForREGISTERCommandPortNum;
extern Boolean forceMulticastOnUnspecified;

extern char const* progName;
extern Boolean supportCodecSelection;

extern TaskToken periodicFileOutputTask;
extern TaskToken sessionTimerTask;
extern TaskToken sessionTimeoutBrokenServerTask;
extern TaskToken arrivalCheckTimerTask;
extern TaskToken interPacketGapCheckTimerTask;
extern TaskToken qosMeasurementTimerTask;

extern MediaSession* session;
extern char const* streamURL;
