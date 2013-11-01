#ifndef CONTAINER_H
#define CONTAINER_H

#include <iostream>
#include <vector>

#include <boost/smart_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/date_time.hpp>


#include "ffmpeg.h"
#include "stream.h"
#include "containerformat.h"
#include "packet.h"
#include "codec.h"

namespace av
{

class Container;
typedef boost::shared_ptr<Container> ContainerPtr;
typedef boost::weak_ptr<Container> ContainerWPtr;


class Stream;
typedef boost::shared_ptr<Stream> StreamPtr;
typedef boost::weak_ptr<Stream> StreamWPtr;


class Packet;
typedef boost::shared_ptr<Packet> PacketPtr;
typedef boost::weak_ptr<Packet> PacketWPtr;



using namespace std;


struct AbstractWriteFunctor
{
public:
    virtual int operator() (uint8_t *buf, int size) = 0;
    virtual const char *name() const = 0;
};


// private data
struct ContainerPriv;

class Container : public boost::enable_shared_from_this<Container>
{
    friend int avioInterruptCallback(void *ptr);

public:
    Container();
    Container(const ContainerFormatPtr &containerFormat);
    virtual ~Container();


    bool openInput(const char *uri, const ContainerFormatPtr &inputFormat = ContainerFormatPtr());
    int32_t readNextPacket(const PacketPtr &pkt);

    void setReadingTimeout(int64_t value);
    int64_t getReadingTimeout() const;


    const StreamPtr addNewStream(const CodecPtr& codec);
    bool openOutput(const char *uri);
    bool openOutput(const AbstractWriteFunctor &writer);

    bool writeHeader();
    int writePacket(const PacketPtr &packet, bool forceInterleaveWrite = false);
    int writeTrailer();


    void flush();
    void close();
    bool isOpened() const;
    int  getStreamsCount() const { return streams.size(); }
    const StreamPtr& getStream(uint32_t index);

    void setFormat(const ContainerFormatPtr &newFormat);
    const ContainerFormatPtr& getFormat() { return format; }

    AVFormatContext *getAVFromatContext();

    void setIndex(uint index);
    uint getIndex() const;

    void dump();

private:
    void setupInterruptHandling();
    int  avioInterruptHandler();

    void init();
    void reset();

    void queryInputStreams();
    void setupInputStreams();

private:
    AVFormatContext    *context;
    ContainerFormatPtr  format;
    vector<StreamPtr>   streams;

    string              uri;

    boost::xtime lastStartReadFrameTime;
    int64_t     readingTimeout;

    ContainerPriv       *priv;
};


} // ::av

#endif // CONTAINER_H
