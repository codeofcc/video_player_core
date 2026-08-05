#include "video.h"


bool Video::open(AVStream* stream, ClockSync* sync, const std::function<void(AVFrame*)>& onFrame, const std::function<void()>& onEof)
{
	_onFrame = onFrame;
	_onEof = onEof;
	_stream = stream;
	_sync = sync;
	if (!_decoder.open(stream, [=](auto frame) {
		while (!_frameQueue.write(&frame, 1)) {
			co_sleep(10);
		}
		})) {
		LOG_ERROR("video decoder init error");
		return false;
	}
	_isAlive = 1;
	_wg.Go([=]() {
		_displayThread();
		});
	return true;
}

void Video::close() {
	_decoder.close();
	_isAlive = 0;
	_wg.Wait();
	AVFrame* frame;

	while (_frameQueue.read(&frame, 1)) {
		av_frame_free(&frame);
	}
	_frameQueue.reset();

	if (_swsContext) {
		sws_freeContext(_swsContext);
		_swsContext = NULL;
	}
	av_frame_free(&_swsFrame);
}

void Video::write(AVPacket* packet)
{
	_decoder.write(packet);
}

void Video::setPixelformat(AVPixelFormat value)
{
	_pixelFormat = value;
}

void Video::_displayThread()
{
	while (_isAlive) {
		AVFrame* frame;
		if (!_frameQueue.read(&frame, 1)) {
			co_sleep(10);
			continue;
		}
		if (!frame) {
			_onEof();
			continue;
		}
		AVRational timebase = _stream->time_base;
		if (frame->pts == AV_NOPTS_VALUE) {
			frame->pts = 0;
		}

		double pts = frame->pts * av_q2d(timebase);
		//printf("video pts %lf\n", pts);
		double duration = frame->duration * av_q2d(timebase);
		double delay = _sync->calVideoDelay(pts, duration);
		while (delay > 0 && _isAlive) {
			if (delay > 0.1) {
				delay = 0.1;
			}
			co_sleep(delay * 1000);
			delay = _sync->calVideoDelay(pts, duration);
		}
		if (delay < 0) {
			av_frame_free(&frame);
			LOG_INFO("drop frame late for %dus", (int)(-delay * 1000000));
			continue;
		}
		//LOG_INFO("video frame pts %lld", pts);
		_sync->updateVideoPts(pts);
		_present(frame);
		av_frame_free(&frame);
	}
}

void Video::_present(AVFrame* frame)
{
	// AVRational timebase = _stream->time_base;
	// double pts = frame->pts * av_q2d(timebase);
	AVFrame* out_frame = frame;
	if (_pixelFormat != AV_PIX_FMT_NONE && _pixelFormat != frame->format) {
		int w = frame->width;
		int h = frame->height;
		_swsContext = sws_getCachedContext(_swsContext,
			w, h, (AVPixelFormat)frame->format,
			w, h, _pixelFormat,
			SWS_FAST_BILINEAR, NULL, NULL, NULL);
		if (!_swsContext) {
			LOG_ERROR("sws_getCachedContext failed");
			return;
		}
		if (!_swsFrame) {
			_swsFrame = av_frame_alloc();
			if (!_swsFrame) {
				LOG_ERROR("swsFrame alloc failed");
				return;
			}
		}
		if (_swsFrame->width != w ||
			_swsFrame->height != h ||
			_swsFrame->format != _pixelFormat) {
			av_frame_unref(_swsFrame);
			_swsFrame->width = w;
			_swsFrame->height = h;
			_swsFrame->format = _pixelFormat;
			if (av_frame_get_buffer(_swsFrame, 32) < 0) {
				LOG_ERROR("swsFrame buffer alloc failed");
				return;
			}
		}
		if (av_frame_make_writable(_swsFrame) < 0) {
			LOG_ERROR("av_frame_make_writable failed");
			return;
		}
		sws_scale(_swsContext,
			(const uint8_t* const*)frame->data, frame->linesize,
			0, frame->height,
			_swsFrame->data, _swsFrame->linesize);
		_swsFrame->pts = frame->pts;
		_swsFrame->duration = frame->duration;
		out_frame = _swsFrame;
	}
	_onFrame(out_frame);
}
