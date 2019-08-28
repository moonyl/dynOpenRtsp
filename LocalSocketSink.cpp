//
// Created by admin on 2019-08-27.
//

#include "LocalSocketSink.h"
#include <QByteArray>
#include <iostream>

LocalSocketSink::LocalSocketSink(UsageEnvironment& env, const char* socketName, unsigned bufferSize,
                   char const* perFrameFileNamePrefix)
        : MediaSink(env), _socketName(socketName), fBufferSize(bufferSize), fSamePresentationTimeCounter(0) {

    _localSocket.abort();
    _localSocket.connectToServer(_socketName);

    fBuffer = new unsigned char[bufferSize];
    if (perFrameFileNamePrefix != nullptr) {
        fPerFrameFileNamePrefix = strDup(perFrameFileNamePrefix);
        fPerFrameFileNameBuffer = new char[strlen(perFrameFileNamePrefix) + 100];
    } else {
        fPerFrameFileNamePrefix = nullptr;
        fPerFrameFileNameBuffer = nullptr;
    }
    fPrevPresentationTime.tv_sec = ~0; fPrevPresentationTime.tv_usec = 0;
}

LocalSocketSink::~LocalSocketSink() {
    delete[] fPerFrameFileNameBuffer;
    delete[] fPerFrameFileNamePrefix;
    delete[] fBuffer;
    //if (fOutFid != nullptr) fclose(fOutFid);
}

LocalSocketSink* LocalSocketSink::createNew(UsageEnvironment& env, char const* socketName,
                              unsigned bufferSize, Boolean oneFilePerFrame) {
//        FILE* fid;
        char const* perFrameFileNamePrefix = nullptr;
        if (oneFilePerFrame) {
//            // Create the fid for each frame
//            fid = nullptr;
            //perFrameFileNamePrefix = fileName;
        } else {
//            // Normal case: create the fid once
//            fid = OpenOutputFile(env, fileName);
//            if (fid == nullptr) break;
            //perFrameFileNamePrefix = nullptr;
        }

        return new LocalSocketSink(env, socketName, bufferSize, perFrameFileNamePrefix);
    //return nullptr;
}

Boolean LocalSocketSink::continuePlaying() {
    if (fSource == nullptr) return False;

    fSource->getNextFrame(fBuffer, fBufferSize,
                          afterGettingFrame, this,
                          onSourceClosure, this);

    return True;
}

void LocalSocketSink::afterGettingFrame(void* clientData, unsigned frameSize,
                                 unsigned numTruncatedBytes,
                                 struct timeval presentationTime,
                                 unsigned /*durationInMicroseconds*/) {
    LocalSocketSink* sink = (LocalSocketSink*)clientData;
    sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime);
}

void LocalSocketSink::addData(unsigned char const* data, unsigned dataSize,
                       struct timeval presentationTime) {
    if (fPerFrameFileNameBuffer != nullptr) {
        // Special case: Open a new file on-the-fly for this frame
        if (presentationTime.tv_usec == fPrevPresentationTime.tv_usec &&
            presentationTime.tv_sec == fPrevPresentationTime.tv_sec) {
            // The presentation time is unchanged from the previous frame, so we add a 'counter'
            // suffix to the file name, to distinguish them:
            sprintf(fPerFrameFileNameBuffer, "%s-%lu.%06lu-%u", fPerFrameFileNamePrefix,
                    presentationTime.tv_sec, presentationTime.tv_usec, ++fSamePresentationTimeCounter);
        } else {
            sprintf(fPerFrameFileNameBuffer, "%s-%lu.%06lu", fPerFrameFileNamePrefix,
                    presentationTime.tv_sec, presentationTime.tv_usec);
            fPrevPresentationTime = presentationTime; // for next time
            fSamePresentationTimeCounter = 0; // for next time
        }
        //fOutFid = OpenOutputFile(envir(), fPerFrameFileNameBuffer);
    }

    // Write to our file:
#ifdef TEST_LOSS
    static unsigned const framesPerPacket = 10;
  static unsigned const frameCount = 0;
  static Boolean const packetIsLost;
  if ((frameCount++)%framesPerPacket == 0) {
    packetIsLost = (our_random()%10 == 0); // simulate 10% packet loss #####
  }

  if (!packetIsLost)
#endif
    if (data != nullptr) {
        //fwrite(data, 1, dataSize, fOutFid);
        qint64 written = _localSocket.write(QByteArray(reinterpret_cast<const char*>(data), dataSize));
        std::cout << "written: " << written << std::endl;
        if (written < 0)    {
            if (fSource != nullptr) fSource->stopGettingFrames();
            onSourceClosure();
            _errorState = "write Error";
            std::cout << qPrintable(_errorState) << std::endl;

        }
    }
}

void LocalSocketSink::afterGettingFrame(unsigned frameSize,
                                 unsigned numTruncatedBytes,
                                 struct timeval presentationTime) {
    if (numTruncatedBytes > 0) {
        envir() << "FileSink::afterGettingFrame(): The input frame data was too large for our buffer size ("
                << fBufferSize << ").  "
                << numTruncatedBytes << " bytes of trailing data was dropped!  Correct this by increasing the \"bufferSize\" parameter in the \"createNew()\" call to at least "
                << fBufferSize + numTruncatedBytes << "\n";
    }
    addData(fBuffer, frameSize, presentationTime);

    if (!_errorState.isEmpty())    {
        return;
    }
//    if (fOutFid == nullptr || fflush(fOutFid) == EOF) {
//        // The output file has closed.  Handle this the same way as if the input source had closed:
//        if (fSource != nullptr) fSource->stopGettingFrames();
//        onSourceClosure();
//        return;
//    }

//    if (fPerFrameFileNameBuffer != nullptr) {
//        if (fOutFid != nullptr) { fclose(fOutFid); fOutFid = nullptr; }
//    }

    // Then try getting the next frame:
    continuePlaying();
}
