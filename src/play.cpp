#include "play.h"

namespace
{
	class _init
	{

	public:
		_init()
		{
			co_sched.goStart(std::thread::hardware_concurrency() * 2);
		}
	};
	_init _;
}

void Play::start(const char *url)
{
	_url = url;
	_isAlive = 1;
	_wg.Go([=]()
		   { _readThread(); });
}
void Play::stop()
{
	_isAlive = 0;
	_wg.Wait();
}

int Play::_interruptCb(void *arg)
{
	auto _this = ((Play *)arg);
	if (!_this->_isPaused)
	{
		CursorTimeChangedEventArgs e{_this->_synchronize->getMasterClock()->getTime()};
		_this->onCursorTimeChanged(_this, &e);
	}
	return !_this->_isAlive;
}
void Play::_readThread()
{
	int videoStreamIndex = -1;
	int audioStreamIndex = -1;
	bool videoEof = false;
	bool audioEof = false;
	int eof = 0;
	_synchronize = std::make_shared<ClockSync>();
	_synchronize->type = CLOCKSYNCTYPE_AUDIO;
	_formatContext = avformat_alloc_context();
	_formatContext->interrupt_callback.callback = Play::_interruptCb;
	_formatContext->interrupt_callback.opaque = this;
	//_formatContext->flags |= AVFMT_FLAG_GENPTS| AVFMT_FLAG_DISCARD_CORRUPT;
	AVDictionary *options = NULL;
	// ǿ��ʹ�� TCP ���䣨interleaved RTP over RTSP��
	// av_dict_set(&options, "rtsp_transport", "tcp", 0);
	av_dict_set(&options, "fflags", "+genpts+discardcorrupt", 0);
	int ret;
	// av_log_set_level(AV_LOG_TRACE);
	if ((ret = avformat_open_input(&_formatContext, _url.c_str(), NULL, NULL)) != 0)
	{

		LOG_ERROR("Couldn't open input stream %d", ret);
		goto end;
	}
	if (avformat_find_stream_info(_formatContext, NULL) < 0)
	{
		LOG_ERROR("Couldn't find stream information");
		goto end;
	}

	for (unsigned i = 0; i < _formatContext->nb_streams; i++)
	{
		enum AVMediaType type = _formatContext->streams[i]->codecpar->codec_type;
		if (type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1)
			videoStreamIndex = i;
		else if (type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
			audioStreamIndex = i;
	}
	if (videoStreamIndex == -1 && audioStreamIndex == -1)
	{
		LOG_ERROR("Didn't find any stream.");
		goto end;
	}

	if (videoStreamIndex != -1)
	{
		if (!_video.open(_formatContext->streams[videoStreamIndex], _synchronize.get(), [=](auto frame)
						 {
			DisplayEventArgs e{ frame };
			onDisplay(this, &e); }, [&]()
						 { videoEof = true; }))
		{
			videoStreamIndex = -1;
		}
	}

	if (audioStreamIndex != -1)
	{
		if (!_audio.open(_formatContext->streams[audioStreamIndex], _synchronize, [&]()
						 {
			audioEof = true;
			_synchronize->type = CLOCKSYNCTYPE_ABSOLUTE; }))
		{
			audioStreamIndex = -1;
		}
	}
	// audioStreamindex = -1;
	if (audioStreamIndex == -1)
	{
		_synchronize->type = CLOCKSYNCTYPE_VIDEO;
	}
	// videoStreamIndex = -1;
	_synchronize->video.isDisabled = videoStreamIndex == -1;
	_synchronize->audio.isDisabled = audioStreamIndex == -1;

	while (_isAlive)
	{
		if (!_isPaused || _step)
		{
			if (!eof)
			{
				AVPacket *packet = av_packet_alloc();
				int ret = av_read_frame(_formatContext, packet);
				if (ret == 0)
				{
					if (packet->stream_index == videoStreamIndex)
						_video.write(packet);
					else if (packet->stream_index == audioStreamIndex)
						_audio.write(packet);
					else
						av_packet_free(&packet);
				}
				else
				{
					eof = ret == AVERROR_EOF;
					if (eof)
					{
						av_packet_free(&packet);
						_video.write(nullptr);
						_audio.write(nullptr);
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				if ((videoStreamIndex == -1 || videoEof) && (audioStreamIndex == -1 || audioEof))
				{
					if (_isLoop)
					{
						avformat_seek_file(_formatContext, -1, INT64_MIN, 0 * AV_TIME_BASE, INT64_MAX, 0);
						avformat_flush(_formatContext);
						eof = 0;
						_synchronize->reset();
						videoEof = false;
						audioEof = false;
						_synchronize->type = CLOCKSYNCTYPE_AUDIO;
					}
				}
				else
				{
					co_sleep(10);
				}
			}
		}
		else
		{
			co_sleep(10);
		}
		/*printf("video time:%lfs %lfs audio time:%lfs  %lfs abs time:%lfs\r", _synchronize->video.getTime(), _synchronize->video.startPts,_synchronize->audio.getTime(), _synchronize->audio.startPts,_synchronize->absolute.getTime());*/
	}
	/*if (_event_callback && (_event_mask & EVENT_PLAY_EOF))
		_event_callback(this, EVENT_PLAY_EOF, NULL);*/
end:
	av_dict_free(&options);
	_video.close();
	_audio.close();
	avformat_close_input(&_formatContext);
}

void Play::setPixelformat(int value)
{
	_video.setPixelformat((AVPixelFormat)value);
}

void Play::setIsPaused(int value)
{
	_isPaused = value;
}

void Play::setVolume(int value)
{
	_audio.setVolume(value);
}

void Play::seek(double value)
{
}
void Play::setSpeed(double value)
{
}
void Play::setIsDisableVideo(int value)
{
}
void Play::setIsDisableAudio(int value)
{
}
void Play::setVideoTrackIndex(int value)
{
}
void Play::setAudioTrackIndex(int value)
{
}