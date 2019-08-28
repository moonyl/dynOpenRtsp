//
// Created by admin on 2019-08-27.
//

#pragma once

#include "json.hpp"
#include <RTSPClient.hh>

class ConfigHoldRTSPClient: public RTSPClient
{
    const nlohmann::json _configData;
    double _duration;
    double _durationSlop;
public:
    static ConfigHoldRTSPClient* createNew(UsageEnvironment& env, char const* rtspURL,
                                           const nlohmann::json& configData, int verbosityLevel = 0,
                                           char const* applicationName = NULL,
                                           portNumBits tunnelOverHTTPPortNum = 0,
                                           int socketNumToServer = -1) {
        return new ConfigHoldRTSPClient(env, rtspURL, configData, verbosityLevel, applicationName, tunnelOverHTTPPortNum, socketNumToServer);
    }

    double duration() const {
        return _duration;
    }

    void setDuration(double duration) {
        _duration = duration;
    }

    double durationSlop() const {
        return _durationSlop;
    }

    double initialSeekTime() const {
        return _configData["initialSeekTime"];
    }
protected:
    ConfigHoldRTSPClient(UsageEnvironment& env, char const* rtspURL, const nlohmann::json& configData,
                         int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum, int socketNumToServer) :
            RTSPClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, socketNumToServer),
            _configData(configData) {
        handleConfigs(configData);
    }
    // called only by createNew();
    virtual ~ConfigHoldRTSPClient() {}

private:
    void handleConfigs(const nlohmann::json& configData) {
        const float value = configData["duration"];
        if (value >= 0) {
            _duration = value;
            _durationSlop = 0;
        } else {
            _duration = 0; // use whatever's in the SDP
            _durationSlop = -value;
        }

        if (_durationSlop < 0) {
            // This parameter wasn't set, so use a default value.
            // If we're measuring QOS stats, then don't add any slop, to avoid
            // having 'empty' measurement intervals at the end.
            _durationSlop = configData["qosMeasurementIntervalMS"] > 0 ? 0.0 : 5.0;
        }
    }
};


