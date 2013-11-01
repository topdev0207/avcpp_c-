#ifndef FRAME_H
#define FRAME_H

#include <vector>

#include <boost/smart_ptr.hpp>

#include "ffmpeg.h"
#include "rational.h"

namespace av
{

/**
 * Base class for VideoFrame and AudioSample. In low both of them is AVFrame, but with some
 * differences, like getSize(), validation logic, setting up data pointers.
 *
 */
class Frame
{
public:
    /**
     * @brief Frame
     *
     * Default ctor
     *
     */
    Frame();
    virtual ~Frame();


    // virtual
    /**
     * Calculate size for frame data, it different for video (avpicture_get_size) and audio
     * (av_samples_get_buffer_size) frames so it need to implemented in extended classes.
     *
     * @return frame data size or -1 if it can not be calculated.
     */
    virtual int                      getSize() const = 0;
    /**
     * Check frame for valid
     * @return true if frame valid false otherwise
     */
    virtual bool                     isValid() const = 0;
    /**
     * Make frame duplicate
     * @return new frame pointer or null if frame can not be created
     */
    virtual boost::shared_ptr<Frame> clone()         = 0;

    // common
    virtual int64_t  getPts() const;
    virtual void     setPts(int64_t pts);

    int64_t getBestEffortTimestamp() const;

    // Some frames must have PTS value setted to AV_NOPTS_VALUE (like audio frames).
    // But, in some situations we should have access to realc or calculated PTS values.
    // So we use FakePts as a name for this PTS values. By default, setPts() methods also set
    // fakePts value but setFakePts() method only set fakePts value.
    virtual int64_t getFakePts() const;
    virtual void    setFakePts(int64_t pts);

    // Non-virtual
    Rational&       getTimeBase() { return timeBase; }
    const Rational& getTimeBase() const { return timeBase; }
    void            setTimeBase(const Rational &value);

    int             getStreamIndex() const;
    void            setStreamIndex(int streamIndex);

    void            setComplete(bool isComplete);
    bool            isComplete() const { return isCompleteFlag; }

    void dump();

    const AVFrame*              getAVFrame() const;
    AVFrame*                    getAVFrame();
    const std::vector<uint8_t>& getFrameBuffer() const;


    // Operators
    Frame& operator= (const AVFrame *frame);
    Frame& operator= (const Frame &frame);


protected:
    // virtual
    virtual void setupDataPointers(const AVFrame *frame) = 0;

    // common
    void initFromAVFrame(const AVFrame *frame);
    void allocFrame();

protected:
    AVFrame*             frame;
    std::vector<uint8_t> frameBuffer;

    Rational             timeBase;
    int                  streamIndex;

    bool                 isCompleteFlag;

    int64_t              fakePts;
};

typedef boost::shared_ptr<Frame> FramePtr;
typedef boost::weak_ptr<Frame> FrameWPtr;

} // ::av

#endif // FRAME_H
