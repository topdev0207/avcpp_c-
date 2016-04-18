#ifndef AV_STREAM_H
#define AV_STREAM_H

#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "timestamp.h"

namespace av
{

enum class Direction
{
    Invalid = -1,
    Encoding,
    Decoding
};


class Stream : public FFWrapperPtr<AVStream>
{
private:
    friend class FormatContext;
    Stream(const std::shared_ptr<char> &monitor, AVStream *st = nullptr, Direction direction = Direction::Invalid);

public:
    Stream() = default;

    bool isValid() const;

    int index() const;
    int id() const;

    Rational    frameRate()          const;
    Rational    timeBase()           const;
    Rational    sampleAspectRatio()  const;
    Timestamp   startTime()          const;
    Timestamp   duration()           const;
    Timestamp   currentDts()         const;
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
    Direction           m_direction = Direction::Invalid;
};

// Back compat alias
using Stream2 attribute_deprecated2("Use `Stream` class (drop-in replacement)") = Stream;

} // ::av

#endif // STREAM_H
