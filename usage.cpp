//
// Created by admin on 2019-08-26.
//
#include "separteExterns.h"
void shutdown(int exitCode = 1);

void usage()
{
    *env << "Usage: " << progName
         << " [-p <startPortNum>] [-r|-q|-4|-i] [-a|-v] [-V] [-d <duration>] [-D <max-inter-packet-gap-time> [-c] [-S <offset>] [-n] [-O]"
         << (controlConnectionUsesTCP ? " [-t|-T <http-port>]" : "")
         << " [-u <username> <password>"
         << (allowProxyServers ? " [<proxy-server> [<proxy-server-port>]]" : "")
         << "]" << (supportCodecSelection ? " [-A <audio-codec-rtp-payload-format-code>|-M <mime-subtype-name>]" : "")
         << " [-s <initial-seek-time>]|[-U <absolute-seek-time>] [-E <absolute-seek-end-time>] [-z <scale>] [-g user-agent]"
         << " [-k <username-for-REGISTER> <password-for-REGISTER>]"
         << " [-P <interval-in-seconds>] [-K]"
         << " [-w <width> -h <height>] [-f <frames-per-second>] [-y] [-H] [-Q [<measurement-interval>]] [-F <filename-prefix>] [-b <file-sink-buffer-size>] [-B <input-socket-buffer-size>] [-I <input-interface-ip-address>] [-m] [<url>|-R [<port-num>]] (or " << progName << " -o [-V] <url>)\n";
    shutdown();
}