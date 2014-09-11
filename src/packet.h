#ifndef PACKET_H
#define PACKET_H

#include <iostream>
#include <vector>

#include "ffmpeg.h"
#include "rational.h"
#include "stream.h"

namespace av {

using namespace std;


typedef std::shared_ptr<class Stream> StreamPtr;
typedef std::weak_ptr<class Stream> StreamWPtr;

typedef std::shared_ptr<class Packet> PacketPtr;
typedef std::weak_ptr<class Packet> PacketWPtr;

class Packet : public FFWrapper<AVPacket>
{
private:
    void initCommon();

    // if deepCopy true - make deep copy, instead - reference is created
    void initFromAVPacket(const AVPacket *avpacket, bool deepCopy);

public:
    Packet();
    Packet(const Packet &packet);
    Packet(Packet &&packet);
    explicit Packet(const AVPacket *packet);
    explicit Packet(const vector<uint8_t> &data);
    Packet(const uint8_t *data, size_t size, bool doAllign = true);
    ~Packet();

    bool setData(const vector<uint8_t> &newData);
    bool setData(const uint8_t *newData, size_t size);

    const uint8_t* getData() const { return m_raw.data; }
    uint8_t*       getData() { return m_raw.data; }

    // Compat
    const AVPacket *getAVPacket() const { return &m_raw; }
    AVPacket       *getAVPacket() {return &m_raw;}

    int64_t getPts() const;
    int64_t getDts() const;
    int64_t getFakePts() const;
    int     getSize() const;

    /**
     * Set packet PTS field. It also set fake pts value, so, if you need fake value, you should
     * store it before and restore later. It useful for audio samples: PTS and DTS values in
     * encoded packets in any cases have AV_NOPTS_VALUE!
     *
     * @param pts new presentation timestamp value
     * @param tsTimeBase  is a time base of setted timestamp, can be omited or sets to Rational(0,0)
     *                    that means that time base equal to packet time base.
     */
    void setPts(int64_t pts,     const Rational &tsTimeBase = Rational(0, 0));
    void setDts(int64_t dts,     const Rational &tsTimeBase = Rational(0, 0));
    void setFakePts(int64_t pts, const Rational &tsTimeBase = Rational(0, 0));

    int     getStreamIndex() const;
    bool    isKeyPacket() const;
    int     getDuration() const;
    bool    isComplete() const;

    void    setStreamIndex(int idx);
    void    setKeyPacket(bool keyPacket);
    void    setDuration(int duration, const Rational &durationTimeBase = Rational(0, 0));
    void    setComplete(bool complete);

    // Flags
    int         getFlags();
    void        setFlags(int flags);
    void        addFlags(int flags);
    void        clearFlags(int flags);

    void        dump(const StreamPtr &st, bool dumpPayload = false) const;

    const Rational& getTimeBase() const { return m_timeBase; }
    void setTimeBase(const Rational &value);

    bool     isReferenced() const;
    int      refCount() const;
    AVPacket makeRef() const;
    Packet   clone() const;

    Packet &operator =(const Packet &rhs);
    Packet &operator= (Packet &&rhs);
    Packet &operator =(const AVPacket &rhs);

    void swap(Packet &other);

private:
    int allocatePayload(int32_t   size);
    int reallocatePayload(int32_t newSize);

private:
    bool             m_completeFlag;
    Rational         m_timeBase;
    int64_t          m_fakePts;
};



} // ::av


#endif // PACKET_H
