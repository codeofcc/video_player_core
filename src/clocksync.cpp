#include "clocksync.h"
#ifdef  __cplusplus
extern "C" {
#endif 
#include "libavutil/time.h"
#ifdef  __cplusplus
}
#endif 
#include<stdio.h>
#include <cmath>
#include <chrono>
static double  getCurrentTime()
{
	//�˴��õ���ffmpeg��av_gettime_relative�����û��ffmpeg����������滻��ƽ̨��ȡʱ�ӵķ�������λΪ�룬������Ҫ΢���Ծ���ʱ�Ӷ����ԡ�
	//return av_gettime_relative() / 1000000.0;
	return std::chrono::time_point_cast <std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count() / 1e+9;
}

void ClockSync::reset() {
	// auto t = type;
	audio = { };
	video = {  };
	absolute = { };

}

Clock* ClockSync::getMasterClock() {
	switch (type)
	{
	case CLOCKSYNCTYPE_AUDIO:
		return &audio;
	case CLOCKSYNCTYPE_VIDEO:
		return &video;
	case CLOCKSYNCTYPE_ABSOLUTE:
		return &absolute;
	default:
		break;
	}
	return 0;
}


double Clock::getTime() {

	return currentPts - startPts;
}

double ClockSync::calVideoDelay(double pts, double duration) {
	//确保所有时钟都已经初始化
	absolute.updatePts(getCurrentTime());
	if (std::isnan(video.startPts)) {
		video.updatePts(pts);
	}
	if (!audio.isDisabled) {
		if (std::isnan(audio.startPts)) {
			return 0.01;
		}
		else if (audio.startPts != video.startPts) {
			//音视频取相同的起点。
			video.updatePts(audio.startPts);
		}
	}

	double targetTime = pts - video.startPts;
	//double correct = video.startPts - getMasterClock()->startPts;
	double currentTime = getMasterClock()->getTime()/*- correct*/;
	//����ʱ���,����0��early��С��0��late��
	double diff = targetTime - currentTime;

	if (diff >= -2 * duration && diff < 0) {
		return 0;
	}
	return diff;

}

double  ClockSync::calAudioDelay(double pts, double duration) {

	absolute.updatePts(getCurrentTime());
	if (std::isnan(audio.startPts)) {
		audio.updatePts(pts);
	}
	if (!video.isDisabled) {
		if (std::isnan(video.startPts)) {
			return 0.01;
		}
	}
	return 0;
}



void Clock::updatePts(double pts) {
	if (std::isnan(startPts))
	{
		startPts = pts;
		printf("start pts %lf\n", startPts);
	}
	currentPts = pts;
}

void ClockSync::updateVideoPts(double pts) {

	absolute.updatePts(getCurrentTime());
	video.updatePts(pts);
}

void ClockSync::updateAudioPts(double pts) {

	absolute.updatePts(getCurrentTime());
	audio.updatePts(pts);
}




