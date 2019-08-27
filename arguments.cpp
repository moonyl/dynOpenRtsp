//
// Created by admin on 2019-08-26.
//
#include <cstdio>
#include "GroupsockHelper.hh"
#include "separteExterns.h"
#include "BasicUsageEnvironment.hh"
#include "json.hpp"
#include <iostream>

void usage();

// duration은 음수로도 설정할 수 있다. durationSlop으로 사용하는데, 아직 의미는 모르겠다.
// interPacketGapMaxTime 은 packet이 오기까지 기다리는 값이라고 되어 있다.
// qosMeasurementIntervalMS 은 100 의 배수이다.
// {
// audioOnly: false, videoOnly: false, verbosityLevel: 1, duration: 0, interPacketGapMaxTime: 0,
// playContinuously: false, oneFilePerFrame: false, streamUsingTCP: false, tunnelOverHTTPPortNum: 0,
// username: "", password: "", usernameForREGISTER: "", passwordForREGISTER: "", sendKeepAlives: false,
// fileNamePrefix: "", fileSinkBufferSize: 100000, socketInputBufferSize: 0, syncStreams: false,
// qosMeasurementIntervalMS: 0, initialSeekTime: 0.0, scale: 1.0
// }

using nlohmann::json;
json configData = {
        {"audioOnly", false}, {"videoOnly", false}, {"verbosityLevel", 1}, {"duration", 0}, {"interPacketGapMaxTime", 0},
        {"playContinuously", false}, {"oneFilePerFrame", false}, {"streamUsingTCP", false}, {"tunnelOverHTTPPortNum", 0},
        {"username", ""}, {"password", ""}, {"usernameForREGISTER", ""}, {"passwordForREGISTER", ""}, {"sendKeepAlives", false},
        {"fileNamePrefix", ""}, {"fileSinkBufferSize", 100000}, {"socketInputBufferSize", 0}, {"syncStreams", false},
        {"qosMeasurementIntervalMS", 0}, {"initialSeekTime", 0.0}, {"scale", 1.0}, {"streamURL", "rtsp://192.168.15.11/Apink_I'mSoSick_720_2000kbps.mp4"}
};
void handleConfigs(/*const json& configData*/)
{
    if (configData["audioOnly"] == true)    {
        singleMedium = "audio";
    } else if (configData["videoOnly"] == true) {
        singleMedium = "video";
    }

    verbosityLevel = configData["verbosityLevel"];
    const float value = configData["duration"];
    if (value >= 0) {
        duration = value;
        durationSlop = 0;
    } else {
        duration = 0; // use whatever's in the SDP
        durationSlop = -value;
    }

    interPacketGapMaxTime = configData["interPacketGapMaxTime"];

    playContinuously = configData["playContinuously"];

    oneFilePerFrame = configData["oneFilePerFrame"];

    streamUsingTCP = configData["streamUsingTCP"];

    tunnelOverHTTPPortNum = configData["tunnelOverHTTPPortNum"];

    const std::string usernameTemp = configData["username"];
    if (usernameTemp.empty())   {
        const std::string passwordTemp = configData["password"];
        ourAuthenticator = new Authenticator(usernameTemp.c_str(), passwordTemp.c_str());
    }

    const std::string usernameForRegisterTemp = configData["usernameForREGISTER"];
    if (usernameForRegisterTemp.empty()) {
        const std::string passwordForRegisterTemp = configData["passwordForREGISTER"];

        if (authDBForREGISTER == nullptr) authDBForREGISTER = new UserAuthenticationDatabase;
        authDBForREGISTER->addUserRecord(usernameForREGISTER, passwordForREGISTER);
    }

    sendKeepAlivesToBrokenServers = configData["sendKeepAlives"];

    fileNamePrefix = configData["fileNamePrefix"];

    fileSinkBufferSize = configData["fileSinkBufferSize"];

    socketInputBufferSize = configData["socketInputBufferSize"];

    syncStreams = configData["syncStreams"];

    qosMeasurementIntervalMS = configData["qosMeasurementIntervalMS"];

    initialSeekTime = configData["initialSeekTime"];

    scale = configData["scale"];

    streamURL = configData["streamURL"];

    if (tunnelOverHTTPPortNum > 0) {
        if (streamUsingTCP) {
            streamUsingTCP = False;
        } else {
            streamUsingTCP = True;
        }
    }

    if (durationSlop < 0) {
        // This parameter wasn't set, so use a default value.
        // If we're measuring QOS stats, then don't add any slop, to avoid
        // having 'empty' measurement intervals at the end.
        durationSlop = qosMeasurementIntervalMS > 0 ? 0.0 : 5.0;
    }

}
