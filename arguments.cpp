//
// Created by admin on 2019-08-26.
//
#include <cstdio>
#include "GroupsockHelper.hh"
#include "separteExterns.h"
#include "BasicUsageEnvironment.hh"

void usage();
// {
//{ startPort: }
// }
void handleArguments(int argc, char** argv)
{
    progName = argv[0];
    // unfortunately we can't use getopt() here, as Windoze doesn't have it
    while (argc > 1) {
        char* const opt = argv[1];
        if (opt[0] != '-') {
            if (argc == 2) break; // only the URL is left
            usage();
        }

        switch (opt[1]) {
            case 'a': { // receive/record an audio stream only
                audioOnly = True;
                singleMedium = "audio";
                break;
            }

            case 'v': { // receive/record a video stream only
                videoOnly = True;
                singleMedium = "video";
                break;
            }

            case 'V': { // disable verbose output
                verbosityLevel = 0;
                break;
            }

            case 'd': { // specify duration, or how much to delay after end time
                float arg;
                if (sscanf(argv[2], "%g", &arg) != 1) {
                    usage();
                }
                if (argv[2][0] == '-') { // not "arg<0", in case argv[2] was "-0"
                    // a 'negative' argument was specified; use this for "durationSlop":
                    duration = 0; // use whatever's in the SDP
                    durationSlop = -arg;
                } else {
                    duration = arg;
                    durationSlop = 0;
                }
                ++argv; --argc;
                break;
            }

            case 'D': { // specify maximum number of seconds to wait for packets:
                if (sscanf(argv[2], "%u", &interPacketGapMaxTime) != 1) {
                    usage();
                }
                ++argv; --argc;
                break;
            }

            case 'c': { // play continuously
                playContinuously = True;
                break;
            }

            case 'm': { // output multiple files - one for each frame
                oneFilePerFrame = True;
                break;
            }

            case 't': {
                // stream RTP and RTCP over the TCP 'control' connection
                if (controlConnectionUsesTCP) {
                    streamUsingTCP = True;
                } else {
                    usage();
                }
                break;
            }

            case 'T': {
                // stream RTP and RTCP over a HTTP connection
                if (controlConnectionUsesTCP) {
                    if (argc > 3 && argv[2][0] != '-') {
                        // The next argument is the HTTP server port number:
                        if (sscanf(argv[2], "%hu", &tunnelOverHTTPPortNum) == 1
                            && tunnelOverHTTPPortNum > 0) {
                            ++argv; --argc;
                            break;
                        }
                    }
                }

                // If we get here, the option was specified incorrectly:
                usage();
                break;
            }

            case 'u': { // specify a username and password
                if (argc < 4) usage(); // there's no argv[3] (for the "password")
                username = argv[2];
                password = argv[3];
                argv+=2; argc-=2;
                if (allowProxyServers && argc > 3 && argv[2][0] != '-') {
                    // The next argument is the name of a proxy server:
                    proxyServerName = argv[2];
                    ++argv; --argc;

                    if (argc > 3 && argv[2][0] != '-') {
                        // The next argument is the proxy server port number:
                        if (sscanf(argv[2], "%hu", &proxyServerPortNum) != 1) {
                            usage();
                        }
                        ++argv; --argc;
                    }
                }

                ourAuthenticator = new Authenticator(username, password);
                break;
            }

            case 'k': { // specify a username and password to be used to authentication an incoming "REGISTER" command (for use with -R)
                if (argc < 4) usage(); // there's no argv[3] (for the "password")
                usernameForREGISTER = argv[2];
                passwordForREGISTER = argv[3];
                argv+=2; argc-=2;

                if (authDBForREGISTER == NULL) authDBForREGISTER = new UserAuthenticationDatabase;
                authDBForREGISTER->addUserRecord(usernameForREGISTER, passwordForREGISTER);
                break;
            }

            case 'K': { // Send periodic 'keep-alive' requests to keep broken server sessions alive
                sendKeepAlivesToBrokenServers = True;
                break;
            }

            case 'F': { // specify a prefix for the audio and video output files
                fileNamePrefix = argv[2];
                ++argv; --argc;
                break;
            }

            case 'b': { // specify the size of buffers for "FileSink"s
                if (sscanf(argv[2], "%u", &fileSinkBufferSize) != 1) {
                    usage();
                }
                ++argv; --argc;
                break;
            }

            case 'B': { // specify the size of input socket buffers
                if (sscanf(argv[2], "%u", &socketInputBufferSize) != 1) {
                    usage();
                }
                ++argv; --argc;
                break;
            }

            case 'y': { // synchronize audio and video streams
                syncStreams = True;
                break;
            }

            case 'Q': { // output QOS measurements
                qosMeasurementIntervalMS = 1000; // default: 1 second

                if (argc > 3 && argv[2][0] != '-') {
                    // The next argument is the measurement interval,
                    // in multiples of 100 ms
                    if (sscanf(argv[2], "%u", &qosMeasurementIntervalMS) != 1) {
                        usage();
                    }
                    qosMeasurementIntervalMS *= 100;
                    ++argv; --argc;
                }
                break;
            }

            case 's': { // specify initial seek time (trick play)
                double arg;
                if (sscanf(argv[2], "%lg", &arg) != 1 || arg < 0) {
                    usage();
                }
                initialSeekTime = arg;
                ++argv; --argc;
                break;
            }

            case 'z': { // scale (trick play)
                float arg;
                if (sscanf(argv[2], "%g", &arg) != 1 || arg == 0.0f) {
                    usage();
                }
                scale = arg;
                ++argv; --argc;
                break;
            }

            default: {
                *env << "Invalid option: " << opt << "\n";
                usage();
                break;
            }
        }

        ++argv; --argc;
    }

    if (audioOnly && videoOnly) {
        *env << "The -a and -v options cannot both be used!\n";
        usage();
    }
    if (sendOptionsRequestOnly && !sendOptionsRequest) {
        *env << "The -o and -O options cannot both be used!\n";
        usage();
    }
    if (initialAbsoluteSeekTime != NULL && initialSeekTime != 0.0f) {
        *env << "The -s and -U options cannot both be used!\n";
        usage();
    }
    if (initialAbsoluteSeekTime == NULL && initialAbsoluteSeekEndTime != NULL) {
        *env << "The -E option requires the -U option!\n";
        usage();
    }
    if (authDBForREGISTER != NULL) {
        *env << "If \"-k <username> <password>\" is used, then -R (or \"-R <port-num>\") must also be used!\n";
        usage();
    }
    if (tunnelOverHTTPPortNum > 0) {
        if (streamUsingTCP) {
            *env << "The -t and -T options cannot both be used!\n";
            usage();
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

    streamURL = argv[1];
}