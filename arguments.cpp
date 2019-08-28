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

using nlohmann::json;

void handleConfigs(const json& configData)
{
    if (configData["audioOnly"] == true)    {
        singleMedium = "audio";
    } else if (configData["videoOnly"] == true) {
        singleMedium = "video";
    }

    interPacketGapMaxTime = configData["interPacketGapMaxTime"];

    playContinuously = configData["playContinuously"];

    oneFilePerFrame = configData["oneFilePerFrame"];

    streamUsingTCP = configData["streamUsingTCP"];
    streamUsingTCP = true;

    tunnelOverHTTPPortNum = configData["tunnelOverHTTPPortNum"];

    const std::string usernameTemp = configData["username"];
    if (!usernameTemp.empty())   {
        const std::string passwordTemp = configData["password"];
        ourAuthenticator = new Authenticator(usernameTemp.c_str(), passwordTemp.c_str());
    }

    const std::string usernameForRegisterTemp = configData["usernameForREGISTER"];
    if (!usernameForRegisterTemp.empty()) {
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

    scale = configData["scale"];

    streamURL = configData["streamURL"];

    if (tunnelOverHTTPPortNum > 0) {
        if (streamUsingTCP) {
            streamUsingTCP = False;
        } else {
            streamUsingTCP = True;
        }
    }
}
