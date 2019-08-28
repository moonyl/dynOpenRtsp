//
// Created by admin on 2019-08-27.
//

#pragma once
#include <MediaSink.hh>
#include <QString>
#include <QLocalSocket>

class LocalSocketSink : public MediaSink {
public:
    static LocalSocketSink* createNew(UsageEnvironment& env, char const* socketName,
                               unsigned bufferSize = 20000,
                               Boolean oneFilePerFrame = False);
    // "bufferSize" should be at least as large as the largest expected
    //   input frame.
    // "oneFilePerFrame" - if True - specifies that each input frame will
    //   be written to a separate file (using the presentation time as a
    //   file name suffix).  The default behavior ("oneFilePerFrame" == False)
    //   is to output all incoming data into a single file.

    virtual void addData(unsigned char const* data, unsigned dataSize,
                         struct timeval presentationTime);
    // (Available in case a client wants to add extra data to the output file)

protected:
    LocalSocketSink(UsageEnvironment& env, const char* socketName, unsigned bufferSize,
    char const* perFrameFileNamePrefix);
    // called only by createNew()
    virtual ~LocalSocketSink();

protected: // redefined virtual functions:
    virtual Boolean continuePlaying();

protected:
    static void afterGettingFrame(void* clientData, unsigned frameSize,
                                  unsigned numTruncatedBytes,
                                  struct timeval presentationTime,
                                  unsigned durationInMicroseconds);
    virtual void afterGettingFrame(unsigned frameSize,
                                   unsigned numTruncatedBytes,
                                   struct timeval presentationTime);

    //FILE* fOutFid;
    QLocalSocket _localSocket;
    QString _socketName;
    unsigned char* fBuffer;
    unsigned fBufferSize;
    char* fPerFrameFileNamePrefix; // used if "oneFilePerFrame" is True
    char* fPerFrameFileNameBuffer; // used if "oneFilePerFrame" is True
    struct timeval fPrevPresentationTime;
    unsigned fSamePresentationTimeCounter;
    QString _errorState;
};



