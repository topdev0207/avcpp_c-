#ifndef AV_BUFFERSRC_H
#define AV_BUFFERSRC_H

#include "ffmpeg.h"
#include "filtercontext.h"
#include "filter.h"
#include "rational.h"
#include "frame.h"
#include "averror.h"

namespace av {

class BufferSrcFilterContext
{
public:
    BufferSrcFilterContext() = default;
    BufferSrcFilterContext(const FilterContext &ctx, std::error_code &ec = throws());

    void assign(const FilterContext &ctx, std::error_code &ec = throws());
    BufferSrcFilterContext& operator=(const FilterContext &ctx);

    void writeVideoFrame(const VideoFrame2 &frame, std::error_code &ec = throws());
    void addVideoFrame(VideoFrame2 &frame, int flags, std::error_code &ec = throws());
    void addVideoFrame(VideoFrame2 &frame, std::error_code &ec = throws());

    void writeAudioSamples(const AudioSamples2 &samples, std::error_code &ec = throws());
    void addAudioSamples(AudioSamples2 &samples, int flags, std::error_code &ec = throws());
    void addAudioSamples(AudioSamples2 &samples, std::error_code &ec = throws());

    size_t failedRequestsCount();

    static FilterMediaType checkFilter(const Filter& filter);

private:
    void addFrame(AVFrame *frame, int flags, std::error_code &ec);
    void writeFrame(const AVFrame *frame, std::error_code &ec);

private:
    FilterContext   m_src;
    FilterMediaType m_type = FilterMediaType::Unknown;
};

} // namespace av

#endif // AV_BUFFERSRC_H
