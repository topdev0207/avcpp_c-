#ifndef AV_STREAM_H
#define AV_STREAM_H

#include <memory>

#include "ffmpeg.h"
#include "rational.h"

namespace av
{

enum class Direction
{
    INVALID = -1,
    ENCODING,
    DECODING
};


class Stream2 : public FFWrapperPtr<AVStream>
{
private:
    friend class FormatContext;
    Stream2(const std::shared_ptr<char> &monitor, AVStream *st = nullptr, Direction direction = Direction::INVALID);

public:
    Stream2() = default;

    bool isValid() const;

    int index() const;
    int id() const;

    Rational    frameRate()          const;
    Rational    timeBase()           const;
    Rational    sampleAspectRatio()  const;
    int64_t     startTime()          const;
    int64_t     duration()           const;
    int64_t     currentDts()         const;
    AVMediaType mediaType()          const;

    bool isAudio()      const;
    bool isVideo()      const;
    bool isData()       const;
    bool isSubtitle()   const;
    bool isAttachment() const;

    Direction   direction() const { return m_direction; }

    void setTimeBase(const Rational &timeBase);
    void setFrameRate(const Rational &frameRate);
    void setSampleAspectRatio(const Rational &aspectRatio);

private:
    std::weak_ptr<char> m_parentMonitor;
    Direction           m_direction = Direction::INVALID;
};


} // ::av

#endif // STREAM_H
