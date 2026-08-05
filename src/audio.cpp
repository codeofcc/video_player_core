#include "audio.h"
#include"SDL.h"
#include"models.h"
#include<atomic>
static enum AVSampleFormat _forceFormat = AV_SAMPLE_FMT_NONE;
static SDL_AudioDeviceID  _audioId;
static SDL_AudioSpec _spec;
static int _frameBytes;
static int _frameAlignBytes;
static intptr_t _audioChannleCount{ 0 };
const co_chan<std::shared_ptr<IMessage>> _audioChan;
static co_mutex _mtx;
std::unordered_set<std::shared_ptr<Audio::Channel>> Audio::_audioChannels;


bool Audio::open(AVStream* stream, const std::shared_ptr<ClockSync>& clock, const std::function<void()>& onEof)
{
	_onEof = onEof;
	_stream = stream;
	if (!_decoder.open(stream, [=](auto frame) {
		if (_isAlive) {
			while (!_frameQueue.write(&frame, 1)) co_sleep(10);
		}
		else {
			av_frame_free(&frame);
		}
		})) {
		LOG_ERROR("video decoder init error");
		return false;
	}
	//if (_forceFormat == AV_SAMPLE_FMT_NONE)
		{
			std::unique_lock<co_mutex> lck(_mtx);
			_audioChannleCount++;
			if (_forceFormat == AV_SAMPLE_FMT_NONE) {
				if (!(SDL_WasInit(0) & (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))) {
					if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
						LOG_ERROR("Could not initialize SDL - %s", SDL_GetError());
						return -1;
					}
				}
				SDL_AudioSpec wantedSpec{ 48000, AUDIO_F32SYS, 2, 0, (Uint16)FFMAX(512, 2 << av_log2(48000 / 30)), 0, 0, Audio::_audioCallback, nullptr };
				_audioId = SDL_OpenAudioDevice(NULL, 0, &wantedSpec, &_spec,
					SDL_AUDIO_ALLOW_ANY_CHANGE);
				if (_audioId < 2) {
					LOG_ERROR("Open audio device error");
					return false;
				}
				switch (_spec.format) {
				case AUDIO_S16SYS: _forceFormat = AV_SAMPLE_FMT_S16; break;
				case AUDIO_S32SYS: _forceFormat = AV_SAMPLE_FMT_S32; break;
				case AUDIO_F32SYS: _forceFormat = AV_SAMPLE_FMT_FLT; break;
				default:
					LOG_ERROR("audio device format not supported %d", (int)_spec.format);
					return false;
				}
				SDL_PauseAudioDevice(_audioId, 0);
				_frameBytes = av_samples_get_buffer_size(0, _spec.channels, _spec.samples, _forceFormat, 1);
				_frameAlignBytes = next_pow2(_frameBytes + 8);
			}
		}

		_channel = std::make_shared<Audio::Channel>(8 * _frameAlignBytes);
		_channel->clock = clock;
		auto msg = std::make_shared<Message<std::shared_ptr<Audio::Channel>>>();
		msg->type = MESSAGE_TYPE_AUDIO_ADD_CHANNEL;
		msg->value = _channel;
		_audioChan << msg;
		_isAlive = 1;
		_wg.Go([=]() {_displayThread(); });
		return true;
}


void Audio::close() {
	_decoder.close();
	_isAlive = 0;
	_wg.Wait();
	AVFrame* frame;
	while (_frameQueue.read(&frame, 1)) {
		av_frame_free(&frame);
	}
	_frameQueue.reset();
	if (_channel) {
		auto msg = std::make_shared<Message<std::shared_ptr<Audio::Channel>>>();
		msg->type = MESSAGE_TYPE_AUDIO_REMOVE_CHANNEL;
		msg->value = _channel;
		_audioChan << msg;
		_channel.reset();
	}
	{
		std::unique_lock<co_mutex> lck(_mtx);
		_audioChannleCount--;
		if (!_audioChannleCount) {
			if (_audioId >= 2) {
				SDL_CloseAudioDevice(_audioId);
				_audioId = 0;
				_forceFormat = AV_SAMPLE_FMT_NONE;
			}
			_audioChannels.clear();
		}
	}
}

void Audio::write(AVPacket* packet)
{
	_decoder.write(packet);
}

void Audio::setVolume(int value)
{
	_volume = value;
}

void Audio::_displayThread() {
	size_t writtenSamples = 0;
	uint8_t* buf = nullptr;
	AVFrame* out = nullptr;
	while (_isAlive) {
		AVFrame* frame;
		int bps;
		size_t avail;
		uint8_t* data;
		int64_t pts;
		if (!_frameQueue.read(&frame, 1)) {
			co_sleep(10);
			continue;
		}
		if (!frame) {
			if (writtenSamples) {
				writtenSamples = 0;
				_channel->frameQueue.end_write(_frameAlignBytes);
			}
			while (_channel->frameQueue.available_read()) {
				co_sleep(10);
			}
			_channel->frameQueue.reset();
			_onEof();
			continue;
		}
		//printf("video time:%lfs %lfs audio time:%lfs  %lfs abs time:%lfs\r", _channel->clock->video.getTime(), _channel->clock->video.startPts, _channel->clock->audio.getTime(), _channel->clock->audio.startPts, _channel->clock->absolute.getTime());
		// �ز���
		if (_forceFormat != frame->format || _spec.freq != frame->sample_rate || _spec.channels != frame->ch_layout.nb_channels) {
			if (!_swrContext) {
				AVChannelLayout out_layout = AV_CHANNEL_LAYOUT_STEREO;
				swr_alloc_set_opts2(&_swrContext, &out_layout, _forceFormat, _spec.freq, &frame->ch_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, nullptr);
				if (!_swrContext || swr_init(_swrContext) < 0) goto end_loop;
			}
			if (!out) {
				out = av_frame_alloc();
				if (!out)goto end_loop;
				out->format = _forceFormat;
				out->sample_rate = _spec.freq;
				out->ch_layout.nb_channels = _spec.channels;
				out->nb_samples = (int64_t)frame->nb_samples * _spec.freq / frame->sample_rate + 256;
				if (av_frame_get_buffer(out, 0) < 0) goto end_loop;
			}
			else {
				out->nb_samples = (int64_t)frame->nb_samples * _spec.freq / frame->sample_rate + 256;
			}
			out->pts = frame->pts == AV_NOPTS_VALUE ? AV_NOPTS_VALUE : av_rescale_q(frame->pts, _stream->time_base, { 1, _spec.freq });
			if ((out->nb_samples = swr_convert(_swrContext, out->data, out->nb_samples, (const uint8_t**)frame->extended_data, frame->nb_samples)) < 0) goto end_loop;
		}
		else {
			out = frame;
		}

		if (_channel->volume != _volume) {
			_channel->volume = _volume;
		}
		bps = av_get_bytes_per_sample((AVSampleFormat)out->format) * out->ch_layout.nb_channels;
		avail = out->nb_samples;
		data = out->data[0];
		pts = out->pts;

		while (avail) {
			if (!writtenSamples) {
				while (!(buf = _channel->frameQueue.begin_write(_frameAlignBytes)))co_sleep(10);
			}
			auto fill = (std::min)(_spec.samples - writtenSamples, avail);
			auto fillBytes = (size_t)fill * bps;
			if (!writtenSamples) {

				*((double*)buf) = (double)pts / _spec.freq;
				double delay;
				//时钟同步
				while ((delay = _channel->clock->calAudioDelay(*((double*)buf), 0)) > 0&& _isAlive)
				{
					LOG_INFO("sleep 10ms wait for video");
					co_sleep(delay * 1000);
				}
			}
			memcpy(buf + 8 + writtenSamples * bps, data, fillBytes);
			writtenSamples += fill;
			data += fillBytes;
			pts += fill;
			avail -= fill;
			if (writtenSamples == _spec.samples) {
				_channel->frameQueue.end_write(_frameAlignBytes);
				writtenSamples = 0;
			}
		}
	end_loop:
		av_frame_free(&frame);
	}
	av_frame_free(&out);
	swr_free(&_swrContext);
}

void SDLCALL Audio::_audioCallback(void* userdata, Uint8* stream, int len)
{
	std::shared_ptr<IMessage> msg;
	while (_audioChan.try_pop(msg)) {
		if (msg->type == MESSAGE_TYPE_AUDIO_ADD_CHANNEL) {
			auto tMsg = (Message<std::shared_ptr<Audio::Channel>>*)(msg.get());
			_audioChannels.insert(tMsg->value);
		}
		else if (msg->type == MESSAGE_TYPE_AUDIO_REMOVE_CHANNEL) {
			auto tMsg = (Message<std::shared_ptr<Audio::Channel>>*)(msg.get());
			_audioChannels.erase(tMsg->value);
		}
	}
	memset(stream, 0, len);
	//auto t = av_gettime_relative();
	//int n = 0;
	for (auto& audio : _audioChannels) {
		auto aBuf = audio->frameQueue.begin_read(_frameAlignBytes);
		if (aBuf) {
			double pts = *((double*)aBuf);
			//printf("device pts:%lf\n", pts);
			audio->clock->updateAudioPts(pts);
			SDL_MixAudioFormat(stream, aBuf + 8, _spec.format, len, audio->volume);
			audio->frameQueue.end_read(_frameAlignBytes);
			//n++;
		}
	}
	//printf("end mix %d %lld\n", n, av_gettime_relative() - t);
}
