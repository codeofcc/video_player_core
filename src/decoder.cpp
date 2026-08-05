#include "decoder.h"


bool Decoder::open(AVStream* stream, const std::function<void(AVFrame*)>& onFrame)
{
	_onFrame = onFrame;
	_codecContext = avcodec_alloc_context3(NULL);
	if (!_codecContext) {
		LOG_ERROR("Could not allocate AVCodecContext");
		return false;
	}
	if (avcodec_parameters_to_context(_codecContext,
		stream->codecpar) < 0) {
		LOG_ERROR("Could not init AVCodecContext");
		return false;
	}
	const AVCodec* codec = avcodec_find_decoder(_codecContext->codec_id);
	if (!codec) {
		LOG_ERROR("Codec not found");
		return false;
	}
	AVDictionary* opts = NULL;
	av_dict_set(&opts, "threads", "1", 0);
	int ret = avcodec_open2(_codecContext, codec, &opts);
	av_dict_free(&opts);
	if (ret < 0) {
		LOG_ERROR("Could not open codec");
		return false;
	}
	_eof = 0;
	_isAlive = 1;
	_wg.Go(
		[=]() {
			AVFrame* frame = av_frame_alloc();
			while (_isAlive) {
				AVPacket* pkt;
				if (!_packetQueue.read(&pkt, 1)) {
					co_sleep(10);
					continue;
				}		
				avcodec_send_packet(_codecContext, pkt);
				while (avcodec_receive_frame(_codecContext, frame) == 0) {
					AVFrame* out = av_frame_alloc();
					av_frame_move_ref(out, frame);
					_onFrame(out);
				}
				if (!pkt) {
					_eof = 1;
					_onFrame(NULL);
					avcodec_flush_buffers(_codecContext);
				}
				else if (_eof) {
					_eof = 0;
				}
				av_packet_free(&pkt);
			}
			av_frame_free(&frame);
		});
	return true;
}

void Decoder::close()
{
	// AVFrame* frame;
	_isAlive = 0;
	_wg.Wait();
	AVPacket* packet;
	while (_packetQueue.read(&packet, 1)) {
		av_packet_free(&packet);
	}
	_packetQueue.reset();
	avcodec_free_context(&_codecContext);
}

void Decoder::write(AVPacket* packet) {
	while (!_packetQueue.write(&packet, 1)) {
		co_sleep(10);
	}
	//printf("%s cache size : % lld\n", _codecContext->codec->name, _packetQueue.available_read());
}