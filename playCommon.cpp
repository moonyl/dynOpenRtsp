/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2019, Live Networks, Inc.  All rights reserved
// A common framework, used for the "openRTSP" and "playSIP" applications
// Implementation
//
// NOTE: If you want to develop your own RTSP client application (or embed RTSP client functionality into your own application),
// then we don't recommend using this code as a model, because it is too complex (with many options).
// Instead, we recommend using the "testRTSPClient" application code as a model.

#include "playCommon.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "arguments.h"
#include <functional>
#include "json.hpp"
#include "ConfigHoldRTSPClient.h"
#include "LocalSocketSink.h"
#if defined(__WIN32__) || defined(_WIN32)
#define snprintf _snprintf
#else
#include <signal.h>
#define USE_SIGNALS 1
#endif


using nlohmann::json;

// Forward function definitions:
void continueAfterClientCreation1(const json& configData);
void continueAfterOPTIONS(RTSPClient* client, int resultCode, char* resultString);
void continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString);
void continueAfterTEARDOWN(RTSPClient* client, int resultCode, char* resultString);

void createOutputFiles(char const* periodicFilenameSuffix);
void createPeriodicOutputFiles();
void setupStreams(ConfigHoldRTSPClient* client);
void closeMediaSinks();
void subsessionAfterPlaying(void* clientData);
void subsessionByeHandler(void* clientData, char const* reason);
void sessionAfterPlaying(void* clientData = nullptr);
void sessionTimerHandler(void* clientData);
void periodicFileOutputTimerHandler(void* clientData);
void shutdown(int exitCode = 1);
void signalHandlerShutdown(int sig);
void checkForPacketArrival(void* clientData);
void checkInterPacketGaps(void* clientData);
void checkSessionTimeoutBrokenServer(void* clientData);
void beginQOSMeasurement();

UsageEnvironment* env;
Medium* ourClient = nullptr;
Authenticator *ourAuthenticator = nullptr;
std::string streamURL = "";
MediaSession* session = nullptr;
TaskToken sessionTimerTask = nullptr;
TaskToken sessionTimeoutBrokenServerTask = nullptr;
TaskToken arrivalCheckTimerTask = nullptr;
TaskToken interPacketGapCheckTimerTask = nullptr;
TaskToken qosMeasurementTimerTask = nullptr;
TaskToken periodicFileOutputTask = nullptr;
QuickTimeFileSink* qtOut = nullptr;
AVIFileSink* aviOut = nullptr;
char const* singleMedium = nullptr;

double initialSeekTime = 0.0f;  // TODO : client 별로
float scale = 1.0f;
double endTime;
unsigned interPacketGapMaxTime = 0;
unsigned totNumPacketsReceived = ~0; // used if checking inter-packet gaps
Boolean playContinuously = False;
int simpleRTPoffsetArg = -1;
Boolean sendOptionsRequest = True;
Boolean oneFilePerFrame = False;
Boolean notifyOnPacketArrival = False;
Boolean sendKeepAlivesToBrokenServers = False;
unsigned sessionTimeoutParameter = 0;
Boolean streamUsingTCP = False;
Boolean forceMulticastOnUnspecified = False;
unsigned short desiredPortNum = 0;
portNumBits tunnelOverHTTPPortNum = 0;
std::string fileNamePrefix = "";
unsigned fileSinkBufferSize = 100000;
unsigned socketInputBufferSize = 0;
Boolean syncStreams = False;
Boolean waitForResponseToTEARDOWN = True;
unsigned qosMeasurementIntervalMS = 0; // 0 means: Don't output QOS data
unsigned fileOutputSecondsSoFar = 0; // seconds
HandlerServerForREGISTERCommand* handlerServerForREGISTERCommand;
char* usernameForREGISTER = nullptr;
char* passwordForREGISTER = nullptr;
UserAuthenticationDatabase* authDBForREGISTER = nullptr;

struct timeval startTime;



class RtspClient
{
    UsageEnvironment &_env;
    std::string _streamURL;
    const json _configData;
    int _verbosityLevel;
    std::string _programName;
    std::function<void(UsageEnvironment &env)> _errorHandler = nullptr;


public:
    RtspClient(UsageEnvironment &env, const std::string& streamUrl, const json& configData, std::function<void(UsageEnvironment &env)> errorHandler = nullptr, int verbosityLevel = 0, const std::string& programName = "") :
            _env(env), _streamURL(streamUrl), _configData(configData), _verbosityLevel(verbosityLevel), _errorHandler(errorHandler), _programName(programName)
    {
    }

    void setVerbosityLevel(int level)   {
        _verbosityLevel = level;
    }

    void setProgramName(const std::string& programName) {
        _programName = programName;
    }

    Medium* create() {
        Medium *result = createClient(_env, _streamURL.c_str(), _configData, _verbosityLevel, _programName.c_str());
        if (result == nullptr && _errorHandler) {
            _errorHandler(_env);
        }
        return result;
    }
};

// duration은 음수로도 설정할 수 있다. durationSlop으로 사용하는데, 아직 의미는 모르겠다.
// interPacketGapMaxTime 은 packet이 오기까지 기다리는 값이라고 되어 있다.
// qosMeasurementIntervalMS 은 100 의 배수이다.
json configData = {
        {"audioOnly", false}, {"videoOnly", false}, {"verbosityLevel", 1}, {"duration", 0}, {"interPacketGapMaxTime", 0},
        {"playContinuously", false}, {"oneFilePerFrame", false}, {"streamUsingTCP", false}, {"tunnelOverHTTPPortNum", 0},
        {"username", ""}, {"password", ""}, {"usernameForREGISTER", ""}, {"passwordForREGISTER", ""}, {"sendKeepAlives", false},
        {"fileNamePrefix", ""}, {"fileSinkBufferSize", 100000}, {"socketInputBufferSize", 0}, {"syncStreams", false},
        {"qosMeasurementIntervalMS", 0}, {"initialSeekTime", 0.0}, {"scale", 1.0}, {"streamURL", "rtsp://192.168.15.11/Apink_I'mSoSick_720_2000kbps.mp4"}
};

int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  gettimeofday(&startTime, nullptr);

#ifdef USE_SIGNALS
  // Allow ourselves to be shut down gracefully by a SIGHUP or a SIGUSR1:
  signal(SIGHUP, signalHandlerShutdown);
  signal(SIGUSR1, signalHandlerShutdown);
#endif
    handleConfigs(configData);

    RtspClient clientCreator(*env, streamURL, configData, [](UsageEnvironment& env) {
        env << "Failed to create " << clientProtocolName << " client: " << env.getResultMsg() << "\n";
        shutdown();
    });
    clientCreator.setProgramName(argv[0]);
    clientCreator.setVerbosityLevel(configData["verbosityLevel"]);
    ourClient = clientCreator.create();
    continueAfterClientCreation1(configData);


  // All subsequent activity takes place within the event loop:
  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void continueAfterClientCreation1(const json &configData) {
  setUserAgentString(nullptr);

  if (sendOptionsRequest) {
    // Begin by sending an "OPTIONS" command:
    getOptions(continueAfterOPTIONS);
  } else {
    continueAfterOPTIONS(nullptr, 0, nullptr);
  }
}

void continueAfterOPTIONS(RTSPClient*, int /*resultCode*/, char* resultString) {
  delete[] resultString;

  // Next, get a SDP description for the stream:
  getSDPDescription(continueAfterDESCRIBE);
}

void continueAfterDESCRIBE(RTSPClient* client, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to get a SDP description for the URL \"" << streamURL.c_str() << "\": " << resultString << "\n";
    delete[] resultString;
    shutdown();
  }

  char* sdpDescription = resultString;
  *env << "Opened URL \"" << streamURL.c_str() << "\", returning a SDP description:\n" << sdpDescription << "\n";

  // Create a media session object from this SDP description:
  session = MediaSession::createNew(*env, sdpDescription);
  delete[] sdpDescription;
  if (session == nullptr) {
    *env << "Failed to create a MediaSession object from the SDP description: " << env->getResultMsg() << "\n";
    shutdown();
  } else if (!session->hasSubsessions()) {
    *env << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
    shutdown();
  }

  // Then, setup the "RTPSource"s for the session:
  MediaSubsessionIterator iter(*session);
  MediaSubsession *subsession;
  Boolean madeProgress = False;
  char const* singleMediumToTest = singleMedium;
  while ((subsession = iter.next()) != nullptr) {
    // If we've asked to receive only a single medium, then check this now:
    if (singleMediumToTest != nullptr) {
      if (strcmp(subsession->mediumName(), singleMediumToTest) != 0) {
		  *env << "Ignoring \"" << subsession->mediumName()
			  << "/" << subsession->codecName()
			  << "\" subsession, because we've asked to receive a single " << singleMedium
			  << " session only\n";
	continue;
      } else {
	// Receive this subsession only
	singleMediumToTest = "xxxxx";
	    // this hack ensures that we get only 1 subsession of this type
      }
    }

    if (desiredPortNum != 0) {
      subsession->setClientPortNum(desiredPortNum);
      desiredPortNum += 2;
    }


      if (!subsession->initiate(simpleRTPoffsetArg)) {
	*env << "Unable to create receiver for \"" << subsession->mediumName()
	     << "/" << subsession->codecName()
	     << "\" subsession: " << env->getResultMsg() << "\n";
      } else {
	*env << "Created receiver for \"" << subsession->mediumName()
	     << "/" << subsession->codecName() << "\" subsession (";
	if (subsession->rtcpIsMuxed()) {
	  *env << "client port " << subsession->clientPortNum();
	} else {
	  *env << "client ports " << subsession->clientPortNum()
	       << "-" << subsession->clientPortNum()+1;
	}
	*env << ")\n";
	madeProgress = True;
	
	if (subsession->rtpSource() != nullptr) {
	  // Because we're saving the incoming data, rather than playing
	  // it in real time, allow an especially large time threshold
	  // (1 second) for reordering misordered incoming packets:
	  unsigned const thresh = 1000000; // 1 second
	  subsession->rtpSource()->setPacketReorderingThresholdTime(thresh);
	  
	  // Set the RTP source's OS socket buffer size as appropriate - either if we were explicitly asked (using -B),
	  // or if the desired FileSink buffer size happens to be larger than the current OS socket buffer size.
	  // (The latter case is a heuristic, on the assumption that if the user asked for a large FileSink buffer size,
	  // then the input data rate may be large enough to justify increasing the OS socket buffer size also.)
	  int socketNum = subsession->rtpSource()->RTPgs()->socketNum();
	  unsigned curBufferSize = getReceiveBufferSize(*env, socketNum);
	  if (socketInputBufferSize > 0 || fileSinkBufferSize > curBufferSize) {
	    unsigned newBufferSize = socketInputBufferSize > 0 ? socketInputBufferSize : fileSinkBufferSize;
	    newBufferSize = setReceiveBufferTo(*env, socketNum, newBufferSize);
	    if (socketInputBufferSize > 0) { // The user explicitly asked for the new socket buffer size; announce it:
	      *env << "Changed socket receive buffer size for the \""
		   << subsession->mediumName()
		   << "/" << subsession->codecName()
		   << "\" subsession from "
		   << curBufferSize << " to "
		   << newBufferSize << " bytes\n";
	    }
	  }
	}
      }

  }
  if (!madeProgress) shutdown();

  // Perform additional 'setup' on each subsession, before playing them:
  setupStreams(dynamic_cast<ConfigHoldRTSPClient*>(client));
}

MediaSubsession *subsession;
Boolean madeProgress = False;
void continueAfterSETUP(RTSPClient* client, int resultCode, char* resultString) {
  if (resultCode == 0) {
      *env << "Setup \"" << subsession->mediumName()
	   << "/" << subsession->codecName()
	   << "\" subsession (";
      if (subsession->rtcpIsMuxed()) {
	*env << "client port " << subsession->clientPortNum();
      } else {
	*env << "client ports " << subsession->clientPortNum()
	     << "-" << subsession->clientPortNum()+1;
      }
      *env << ")\n";
      madeProgress = True;
  } else {
    *env << "Failed to setup \"" << subsession->mediumName()
	 << "/" << subsession->codecName()
	 << "\" subsession: " << resultString << "\n";
  }
  delete[] resultString;

  if (client != nullptr) sessionTimeoutParameter = client->sessionTimeoutParameter();

  // Set up the next subsession, if any:
  setupStreams(dynamic_cast<ConfigHoldRTSPClient*>(client));
}

void createOutputFiles(char const* periodicFilenameSuffix) {
  char outFileName[1000];

    // Create and start "FileSink"s for each subsession:
    madeProgress = False;
    MediaSubsessionIterator iter(*session);
    while ((subsession = iter.next()) != nullptr) {
      if (subsession->readSource() == nullptr) continue; // was not initiated
      
      // Create an output file for each desired stream:
      if (singleMedium == nullptr || periodicFilenameSuffix[0] != '\0') {
	// Output file name is
	//     "<filename-prefix><medium_name>-<codec_name>-<counter><periodicFilenameSuffix>"
	static unsigned streamCounter = 0;
	snprintf(outFileName, sizeof outFileName, "%s%s-%s-%d%s",
		 fileNamePrefix.c_str(), subsession->mediumName(),
		 subsession->codecName(), ++streamCounter, periodicFilenameSuffix);
      } else {
	// When outputting a single medium only, we output to 'stdout
	// (unless the '-P <interval-in-seconds>' option was given):
	sprintf(outFileName, "stdout");
      }

      FileSink* fileSink = nullptr;
      Boolean createOggFileSink = False; // by default
      if (strcmp(subsession->mediumName(), "video") == 0) {
	if (strcmp(subsession->codecName(), "H264") == 0) {
	  // For H.264 video stream, we use a special sink that adds 'start codes',
	  // and (at the start) the SPS and PPS NAL units:
	  fileSink = H264VideoFileSink::createNew(*env, outFileName,
						  subsession->fmtp_spropparametersets(),
						  fileSinkBufferSize, oneFilePerFrame);
	} else if (strcmp(subsession->codecName(), "H265") == 0) {
	  // For H.265 video stream, we use a special sink that adds 'start codes',
	  // and (at the start) the VPS, SPS, and PPS NAL units:
	  fileSink = H265VideoFileSink::createNew(*env, outFileName,
						  subsession->fmtp_spropvps(),
						  subsession->fmtp_spropsps(),
						  subsession->fmtp_sproppps(),
						  fileSinkBufferSize, oneFilePerFrame);
	} else if (strcmp(subsession->codecName(), "THEORA") == 0) {
	  createOggFileSink = True;
	}
      } else if (strcmp(subsession->mediumName(), "audio") == 0) {
	if (strcmp(subsession->codecName(), "AMR") == 0 ||
	    strcmp(subsession->codecName(), "AMR-WB") == 0) {
	  // For AMR audio streams, we use a special sink that inserts AMR frame hdrs:
	  fileSink = AMRAudioFileSink::createNew(*env, outFileName,
						 fileSinkBufferSize, oneFilePerFrame);
	} else if (strcmp(subsession->codecName(), "VORBIS") == 0 ||
		   strcmp(subsession->codecName(), "OPUS") == 0) {
	  createOggFileSink = True;
	}
      }
        LocalSocketSink*  localSocketSink = nullptr;
      if (createOggFileSink) {
	fileSink = OggFileSink
	  ::createNew(*env, outFileName,
		      subsession->rtpTimestampFrequency(), subsession->fmtp_config());
      } else if (fileSink == nullptr) {
	// Normal case:
	//fileSink = FileSink::createNew(*env, outFileName,
	//			       fileSinkBufferSize, oneFilePerFrame);
          localSocketSink = LocalSocketSink::createNew(*env, "audioStream", fileSinkBufferSize, oneFilePerFrame);
      }
        if(localSocketSink) {
            subsession->sink = localSocketSink;
        }
        //subsession->sink = fileSink;

      if (subsession->sink == nullptr) {
	*env << "Failed to create FileSink for \"" << outFileName
	     << "\": " << env->getResultMsg() << "\n";
      } else {
	if (singleMedium == nullptr) {
	  *env << "Created output file: \"" << outFileName << "\"\n";
	} else {
	  *env << "Outputting data from the \"" << subsession->mediumName()
	       << "/" << subsession->codecName()
	       << "\" subsession to \"" << outFileName << "\"\n";
	}
	
	if (strcmp(subsession->mediumName(), "video") == 0 &&
	    strcmp(subsession->codecName(), "MP4V-ES") == 0 &&
	    subsession->fmtp_config() != nullptr) {
	  // For MPEG-4 video RTP streams, the 'config' information
	  // from the SDP description contains useful VOL etc. headers.
	  // Insert this data at the front of the output file:
	  unsigned configLen;
	  unsigned char* configData
	    = parseGeneralConfigStr(subsession->fmtp_config(), configLen);
	  struct timeval timeNow;
	  gettimeofday(&timeNow, nullptr);
	  fileSink->addData(configData, configLen, timeNow);
	  delete[] configData;
	}
	
	subsession->sink->startPlaying(*(subsession->readSource()),
				       subsessionAfterPlaying,
				       subsession);
	
	// Also set a handler to be called if a RTCP "BYE" arrives
	// for this subsession:
	if (subsession->rtcpInstance() != nullptr) {
	  subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, subsession);
	}
	
	madeProgress = True;
      }
    }
    if (!madeProgress) shutdown();

}

void createPeriodicOutputFiles() {
  // Create a filename suffix that notes the time interval that's being recorded:
  char periodicFileNameSuffix[100];
  snprintf(periodicFileNameSuffix, sizeof periodicFileNameSuffix, "-%05d-%05d",
	   fileOutputSecondsSoFar, fileOutputSecondsSoFar + 0);
  createOutputFiles(periodicFileNameSuffix);

  // Schedule an event for writing the next output file:
  periodicFileOutputTask
    = env->taskScheduler().scheduleDelayedTask(0,
					       (TaskFunc*)periodicFileOutputTimerHandler,
					       (void*)nullptr);
}

void setupStreams(ConfigHoldRTSPClient* client) {
  static MediaSubsessionIterator* setupIter = nullptr;
  if (setupIter == nullptr) setupIter = new MediaSubsessionIterator(*session);
  while ((subsession = setupIter->next()) != nullptr) {
    // We have another subsession left to set up:
    if (subsession->clientPortNum() == 0) continue; // port # was not set

    setupSubsession(subsession, streamUsingTCP, forceMulticastOnUnspecified, continueAfterSETUP);
    return;
  }

  // We're done setting up subsessions.
  delete setupIter;
  if (!madeProgress) shutdown();

  // Create output files:
   createOutputFiles("");

  // Finally, start playing each subsession, to start the data flow:
  if (client->duration() == 0) {
    if (scale > 0) client->setDuration(session->playEndTime()); // use SDP end time
    else if (scale < 0) client->setDuration(0.0f);
  }
  if (client->duration() < 0) client->setDuration(0.0);

  endTime = 0.0f;
  if (scale > 0) {
    if (client->duration() <= 0) endTime = -1.0f;
    else endTime = client->duration();
  } else {
    endTime = 0.0f - client->duration();
    if (endTime < 0) endTime = 0.0f;
  }

  char const* absStartTime = session->absStartTime();
  char const* absEndTime = session->absEndTime();
  if (absStartTime != nullptr) {
    // Either we or the server have specified that seeking should be done by 'absolute' time:
    startPlayingSession(session, absStartTime, absEndTime, scale, continueAfterPLAY);
  } else {
    // Normal case: Seek by relative time (NPT):
    startPlayingSession(session, 0.0f, endTime, scale, continueAfterPLAY);
  }
}

void continueAfterPLAY(RTSPClient* client, int resultCode, char* resultString) {
  if (resultCode != 0) {
    *env << "Failed to start playing session: " << resultString << "\n";
    delete[] resultString;
    shutdown();
    return;
  } else {
    *env << "Started playing session\n";
  }
  delete[] resultString;

  if (qosMeasurementIntervalMS > 0) {
    // Begin periodic QOS measurements:
    beginQOSMeasurement();
  }

  // Figure out how long to delay (if at all) before shutting down, or
  // repeating the playing
  Boolean timerIsBeingUsed = False;

  ConfigHoldRTSPClient* holdRtspClient = dynamic_cast<ConfigHoldRTSPClient*>(client);
  double secondsToDelay = holdRtspClient->duration();
  if (holdRtspClient->duration() > 0) {
    // First, adjust "duration" based on any change to the play range (that was specified in the "PLAY" response):
    double rangeAdjustment = (session->playEndTime() - session->playStartTime()) - endTime;
    if (holdRtspClient->duration() + rangeAdjustment > 0.0) holdRtspClient->setDuration(holdRtspClient->duration() + rangeAdjustment);

    timerIsBeingUsed = True;
    double absScale = scale > 0 ? scale : -scale; // ASSERT: scale != 0
    secondsToDelay = holdRtspClient->duration()/absScale + holdRtspClient->durationSlop();

    int64_t uSecsToDelay = (int64_t)(secondsToDelay*1000000.0);
    sessionTimerTask = env->taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)sessionTimerHandler, (void*)client);
  }

  char const* actionString
    = "Receiving streamed data";
  if (timerIsBeingUsed) {
    *env << actionString
		<< " (for up to " << secondsToDelay
		<< " seconds)...\n";
  } else {
#ifdef USE_SIGNALS
    pid_t ourPid = getpid();
    *env << actionString
		<< " (signal with \"kill -HUP " << (int)ourPid
		<< "\" or \"kill -USR1 " << (int)ourPid
		<< "\" to terminate)...\n";
#else
    *env << actionString << "...\n";
#endif
  }

  sessionTimeoutBrokenServerTask = nullptr;

  // Watch for incoming packets (if desired):
  checkForPacketArrival(nullptr);
  checkInterPacketGaps(client);
  checkSessionTimeoutBrokenServer(nullptr);
}

void closeMediaSinks() {
  Medium::close(qtOut); qtOut = nullptr;
  Medium::close(aviOut); aviOut = nullptr;

  if (session == nullptr) return;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != nullptr) {
    Medium::close(subsession->sink);
    subsession->sink = nullptr;
  }
}

void subsessionAfterPlaying(void* clientData) {
  // Begin by closing this media subsession's stream:
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  Medium::close(subsession->sink);
  subsession->sink = nullptr;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != nullptr) {
    if (subsession->sink != nullptr) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed
  sessionAfterPlaying(clientData);
}

void subsessionByeHandler(void* clientData, char const* reason) {
  struct timeval timeNow;
  gettimeofday(&timeNow, nullptr);
  unsigned secsDiff = timeNow.tv_sec - startTime.tv_sec;

  MediaSubsession* subsession = (MediaSubsession*)clientData;
  *env << "Received RTCP \"BYE\"";
  if (reason != nullptr) {
    *env << " (reason:\"" << reason << "\")";
    delete[] reason;
  }
  *env << " on \"" << subsession->mediumName()
	<< "/" << subsession->codecName()
	<< "\" subsession (after " << secsDiff
	<< " seconds)\n";

  // Act now as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void sessionAfterPlaying(void* /*clientData*/) {
  if (!playContinuously) {
    shutdown(0);
  } else {
    // We've been asked to play the stream(s) over again.
    // First, reset state from the current session:
    if (env != nullptr) {
      // Keep this running:      env->taskScheduler().unscheduleDelayedTask(periodicFileOutputTask);
      env->taskScheduler().unscheduleDelayedTask(sessionTimerTask);
      env->taskScheduler().unscheduleDelayedTask(sessionTimeoutBrokenServerTask);
      env->taskScheduler().unscheduleDelayedTask(arrivalCheckTimerTask);
      env->taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
      env->taskScheduler().unscheduleDelayedTask(qosMeasurementTimerTask);
    }
    totNumPacketsReceived = ~0;

    //ConfigHoldRTSPClient* client = static_cast<ConfigHoldRTSPClient*>(clientData);
    startPlayingSession(session, 0.0f, endTime, scale, continueAfterPLAY);
  }
}

void sessionTimerHandler(void* clientData) {
  sessionTimerTask = nullptr;

  sessionAfterPlaying(clientData);
}

void periodicFileOutputTimerHandler(void* /*clientData*/) {
  fileOutputSecondsSoFar += 0;

  // First, close the existing output files:
  closeMediaSinks();

  // Then, create new output files:
  createPeriodicOutputFiles();
}

class qosMeasurementRecord {
public:
  qosMeasurementRecord(struct timeval const& startTime, RTPSource* src)
    : fSource(src), fNext(nullptr),
      kbits_per_second_min(1e20), kbits_per_second_max(0),
      kBytesTotal(0.0),
      packet_loss_fraction_min(1.0), packet_loss_fraction_max(0.0),
      totNumPacketsReceived(0), totNumPacketsExpected(0) {
    measurementEndTime = measurementStartTime = startTime;

    RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
    // Assume that there's only one SSRC source (usually the case):
    RTPReceptionStats* stats = statsIter.next(True);
    if (stats != nullptr) {
      kBytesTotal = stats->totNumKBytesReceived();
      totNumPacketsReceived = stats->totNumPacketsReceived();
      totNumPacketsExpected = stats->totNumPacketsExpected();
    }
  }
  virtual ~qosMeasurementRecord() { delete fNext; }

  void periodicQOSMeasurement(struct timeval const& timeNow);

public:
  RTPSource* fSource;
  qosMeasurementRecord* fNext;

public:
  struct timeval measurementStartTime, measurementEndTime;
  double kbits_per_second_min, kbits_per_second_max;
  double kBytesTotal;
  double packet_loss_fraction_min, packet_loss_fraction_max;
  unsigned totNumPacketsReceived, totNumPacketsExpected;
};

static qosMeasurementRecord* qosRecordHead = nullptr;

static void periodicQOSMeasurement(void* clientData); // forward

static unsigned nextQOSMeasurementUSecs;

static void scheduleNextQOSMeasurement() {
  nextQOSMeasurementUSecs += qosMeasurementIntervalMS*1000;
  struct timeval timeNow;
  gettimeofday(&timeNow, nullptr);
  unsigned timeNowUSecs = timeNow.tv_sec*1000000 + timeNow.tv_usec;
  int usecsToDelay = nextQOSMeasurementUSecs - timeNowUSecs;

  qosMeasurementTimerTask = env->taskScheduler().scheduleDelayedTask(
     usecsToDelay, (TaskFunc*)periodicQOSMeasurement, (void*)nullptr);
}

static void periodicQOSMeasurement(void* /*clientData*/) {
  struct timeval timeNow;
  gettimeofday(&timeNow, nullptr);

  for (qosMeasurementRecord* qosRecord = qosRecordHead;
       qosRecord != nullptr; qosRecord = qosRecord->fNext) {
    qosRecord->periodicQOSMeasurement(timeNow);
  }

  // Do this again later:
  scheduleNextQOSMeasurement();
}

void qosMeasurementRecord
::periodicQOSMeasurement(struct timeval const& timeNow) {
  unsigned secsDiff = timeNow.tv_sec - measurementEndTime.tv_sec;
  int usecsDiff = timeNow.tv_usec - measurementEndTime.tv_usec;
  double timeDiff = secsDiff + usecsDiff/1000000.0;
  measurementEndTime = timeNow;

  RTPReceptionStatsDB::Iterator statsIter(fSource->receptionStatsDB());
  // Assume that there's only one SSRC source (usually the case):
  RTPReceptionStats* stats = statsIter.next(True);
  if (stats != nullptr) {
    double kBytesTotalNow = stats->totNumKBytesReceived();
    double kBytesDeltaNow = kBytesTotalNow - kBytesTotal;
    kBytesTotal = kBytesTotalNow;

    double kbpsNow = timeDiff == 0.0 ? 0.0 : 8*kBytesDeltaNow/timeDiff;
    if (kbpsNow < 0.0) kbpsNow = 0.0; // in case of roundoff error
    if (kbpsNow < kbits_per_second_min) kbits_per_second_min = kbpsNow;
    if (kbpsNow > kbits_per_second_max) kbits_per_second_max = kbpsNow;

    unsigned totReceivedNow = stats->totNumPacketsReceived();
    unsigned totExpectedNow = stats->totNumPacketsExpected();
    unsigned deltaReceivedNow = totReceivedNow - totNumPacketsReceived;
    unsigned deltaExpectedNow = totExpectedNow - totNumPacketsExpected;
    totNumPacketsReceived = totReceivedNow;
    totNumPacketsExpected = totExpectedNow;

    double lossFractionNow = deltaExpectedNow == 0 ? 0.0
      : 1.0 - deltaReceivedNow/(double)deltaExpectedNow;
    //if (lossFractionNow < 0.0) lossFractionNow = 0.0; //reordering can cause
    if (lossFractionNow < packet_loss_fraction_min) {
      packet_loss_fraction_min = lossFractionNow;
    }
    if (lossFractionNow > packet_loss_fraction_max) {
      packet_loss_fraction_max = lossFractionNow;
    }
  }
}

void beginQOSMeasurement() {
  // Set up a measurement record for each active subsession:
  struct timeval startTime;
  gettimeofday(&startTime, nullptr);
  nextQOSMeasurementUSecs = startTime.tv_sec*1000000 + startTime.tv_usec;
  qosMeasurementRecord* qosRecordTail = nullptr;
  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != nullptr) {
    RTPSource* src = subsession->rtpSource();
    if (src == nullptr) continue;

    qosMeasurementRecord* qosRecord
      = new qosMeasurementRecord(startTime, src);
    if (qosRecordHead == nullptr) qosRecordHead = qosRecord;
    if (qosRecordTail != nullptr) qosRecordTail->fNext = qosRecord;
    qosRecordTail  = qosRecord;
  }

  // Then schedule the first of the periodic measurements:
  scheduleNextQOSMeasurement();
}

void printQOSData(int /*exitCode*/) {
  *env << "begin_QOS_statistics\n";
  
  // Print out stats for each active subsession:
  qosMeasurementRecord* curQOSRecord = qosRecordHead;
  if (session != nullptr) {
    MediaSubsessionIterator iter(*session);
    MediaSubsession* subsession;
    while ((subsession = iter.next()) != nullptr) {
      RTPSource* src = subsession->rtpSource();
      if (src == nullptr) continue;
      
      *env << "subsession\t" << subsession->mediumName()
	   << "/" << subsession->codecName() << "\n";
      
      unsigned numPacketsReceived = 0, numPacketsExpected = 0;
      
      if (curQOSRecord != nullptr) {
	numPacketsReceived = curQOSRecord->totNumPacketsReceived;
	numPacketsExpected = curQOSRecord->totNumPacketsExpected;
      }
      *env << "num_packets_received\t" << numPacketsReceived << "\n";
      *env << "num_packets_lost\t" << int(numPacketsExpected - numPacketsReceived) << "\n";
      
      if (curQOSRecord != nullptr) {
	unsigned secsDiff = curQOSRecord->measurementEndTime.tv_sec
	  - curQOSRecord->measurementStartTime.tv_sec;
	int usecsDiff = curQOSRecord->measurementEndTime.tv_usec
	  - curQOSRecord->measurementStartTime.tv_usec;
	double measurementTime = secsDiff + usecsDiff/1000000.0;
	*env << "elapsed_measurement_time\t" << measurementTime << "\n";
	
	*env << "kBytes_received_total\t" << curQOSRecord->kBytesTotal << "\n";
	
	*env << "measurement_sampling_interval_ms\t" << qosMeasurementIntervalMS << "\n";
	
	if (curQOSRecord->kbits_per_second_max == 0) {
	  // special case: we didn't receive any data:
	  *env <<
	    "kbits_per_second_min\tunavailable\n"
	    "kbits_per_second_ave\tunavailable\n"
	    "kbits_per_second_max\tunavailable\n";
	} else {
	  *env << "kbits_per_second_min\t" << curQOSRecord->kbits_per_second_min << "\n";
	  *env << "kbits_per_second_ave\t"
	       << (measurementTime == 0.0 ? 0.0 : 8*curQOSRecord->kBytesTotal/measurementTime) << "\n";
	  *env << "kbits_per_second_max\t" << curQOSRecord->kbits_per_second_max << "\n";
	}
	
	*env << "packet_loss_percentage_min\t" << 100*curQOSRecord->packet_loss_fraction_min << "\n";
	double packetLossFraction = numPacketsExpected == 0 ? 1.0
	  : 1.0 - numPacketsReceived/(double)numPacketsExpected;
	if (packetLossFraction < 0.0) packetLossFraction = 0.0;
	*env << "packet_loss_percentage_ave\t" << 100*packetLossFraction << "\n";
	*env << "packet_loss_percentage_max\t"
	     << (packetLossFraction == 1.0 ? 100.0 : 100*curQOSRecord->packet_loss_fraction_max) << "\n";
	
	RTPReceptionStatsDB::Iterator statsIter(src->receptionStatsDB());
	// Assume that there's only one SSRC source (usually the case):
	RTPReceptionStats* stats = statsIter.next(True);
	if (stats != nullptr) {
	  *env << "inter_packet_gap_ms_min\t" << stats->minInterPacketGapUS()/1000.0 << "\n";
	  struct timeval totalGaps = stats->totalInterPacketGaps();
	  double totalGapsMS = totalGaps.tv_sec*1000.0 + totalGaps.tv_usec/1000.0;
	  unsigned totNumPacketsReceived = stats->totNumPacketsReceived();
	  *env << "inter_packet_gap_ms_ave\t"
	       << (totNumPacketsReceived == 0 ? 0.0 : totalGapsMS/totNumPacketsReceived) << "\n";
	  *env << "inter_packet_gap_ms_max\t" << stats->maxInterPacketGapUS()/1000.0 << "\n";
	}
	
	curQOSRecord = curQOSRecord->fNext;
      }
    }
  }

  *env << "end_QOS_statistics\n";
  delete qosRecordHead;
}



void continueAfterTEARDOWN(RTSPClient*, int /*resultCode*/, char* resultString) {
  delete[] resultString;

  // Now that we've stopped any more incoming data from arriving, close our output files:
  closeMediaSinks();
  Medium::close(session);

  // Finally, shut down our client:
  delete ourAuthenticator;
  delete authDBForREGISTER;
  Medium::close(ourClient);

  // Adios...
  exit(shutdownExitCode);
}

void signalHandlerShutdown(int /*sig*/) {
  *env << "Got shutdown signal\n";
  waitForResponseToTEARDOWN = False; // to ensure that we end, even if the server does not respond to our TEARDOWN
  shutdown(0);
}

void checkForPacketArrival(void* /*clientData*/) {
  if (!notifyOnPacketArrival) return; // we're not checking

  // Check each subsession, to see whether it has received data packets:
  unsigned numSubsessionsChecked = 0;
  unsigned numSubsessionsWithReceivedData = 0;
  unsigned numSubsessionsThatHaveBeenSynced = 0;

  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != nullptr) {
    RTPSource* src = subsession->rtpSource();
    if (src == nullptr) continue;
    ++numSubsessionsChecked;

    if (src->receptionStatsDB().numActiveSourcesSinceLastReset() > 0) {
      // At least one data packet has arrived
      ++numSubsessionsWithReceivedData;
    }
    if (src->hasBeenSynchronizedUsingRTCP()) {
      ++numSubsessionsThatHaveBeenSynced;
    }
  }

  unsigned numSubsessionsToCheck = numSubsessionsChecked;
  // Special case for "QuickTimeFileSink"s and "AVIFileSink"s:
  // They might not use all of the input sources:
  if (qtOut != nullptr) {
    numSubsessionsToCheck = qtOut->numActiveSubsessions();
  } else if (aviOut != nullptr) {
    numSubsessionsToCheck = aviOut->numActiveSubsessions();
  }

  Boolean notifyTheUser;
  if (!syncStreams) {
    notifyTheUser = numSubsessionsWithReceivedData > 0; // easy case
  } else {
    notifyTheUser = numSubsessionsWithReceivedData >= numSubsessionsToCheck
      && numSubsessionsThatHaveBeenSynced == numSubsessionsChecked;
    // Note: A subsession with no active sources is considered to be synced
  }
  if (notifyTheUser) {
    struct timeval timeNow;
    gettimeofday(&timeNow, nullptr);
	char timestampStr[100];
	sprintf(timestampStr, "%ld%03ld", timeNow.tv_sec, (long)(timeNow.tv_usec/1000));
    *env << (syncStreams ? "Synchronized d" : "D")
		<< "ata packets have begun arriving [" << timestampStr << "]\007\n";
    return;
  }

  // No luck, so reschedule this check again, after a delay:
  int uSecsToDelay = 100000; // 100 ms
  arrivalCheckTimerTask
    = env->taskScheduler().scheduleDelayedTask(uSecsToDelay,
			       (TaskFunc*)checkForPacketArrival, nullptr);
}

void checkInterPacketGaps(void* clientData) {
  if (interPacketGapMaxTime == 0) return; // we're not checking

  // Check each subsession, counting up how many packets have been received:
  unsigned newTotNumPacketsReceived = 0;

  MediaSubsessionIterator iter(*session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != nullptr) {
    RTPSource* src = subsession->rtpSource();
    if (src == nullptr) continue;
    newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
  }

  if (newTotNumPacketsReceived == totNumPacketsReceived) {
    // No additional packets have been received since the last time we
    // checked, so end this stream:
    *env << "Closing session, because we stopped receiving packets.\n";
    interPacketGapCheckTimerTask = nullptr;
    sessionAfterPlaying(clientData);
  } else {
    totNumPacketsReceived = newTotNumPacketsReceived;
    // Check again, after the specified delay:
    interPacketGapCheckTimerTask
      = env->taskScheduler().scheduleDelayedTask(interPacketGapMaxTime*1000000,
				 (TaskFunc*)checkInterPacketGaps, clientData);
  }
}

void checkSessionTimeoutBrokenServer(void* /*clientData*/) {
  if (!sendKeepAlivesToBrokenServers) return; // we're not checking

  // Send an "OPTIONS" request, starting with the second call
  if (sessionTimeoutBrokenServerTask != nullptr) {
    getOptions(nullptr);
  }
  
  unsigned sessionTimeout = sessionTimeoutParameter == 0 ? 60/*default*/ : sessionTimeoutParameter;
  unsigned secondsUntilNextKeepAlive = sessionTimeout <= 5 ? 1 : sessionTimeout - 5;
      // Reduce the interval a little, to be on the safe side

  sessionTimeoutBrokenServerTask 
    = env->taskScheduler().scheduleDelayedTask(secondsUntilNextKeepAlive*1000000,
			 (TaskFunc*)checkSessionTimeoutBrokenServer, nullptr);
					       
}
