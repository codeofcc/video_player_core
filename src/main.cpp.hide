
#include"play.h"


static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;
static void handle_sdl_events(int* running)
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			*running = 0;
		}
		else if (event.type == SDL_KEYDOWN) {
			if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_q) {
				*running = 0;
			}
		}
	}
}

static void cleanup_sdl(void)
{
	if (g_texture) { SDL_DestroyTexture(g_texture);  g_texture = NULL; }
	if (g_renderer) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
	if (g_window) { SDL_DestroyWindow(g_window);    g_window = NULL; }
}


#undef main
int main(int argc, char** argv)
{

	//co_sched.goStart(std::thread::hardware_concurrency() * 2);
	//if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
	//	LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
	//	return -1;
	//}
	//for (int i = 0; i < 128;i++) {
	//	Play *play=new Play;
	//	//memset(&play, 0, sizeof(Play));
	//	play->isLoop = 1;
	//	play->video.forcePixelFormat = AV_PIX_FMT_YUV420P;
	//	/*play_set_event_callback(&play, my_event_callback,
	//		EVENT_VIDEO_FRAME | EVENT_PLAY_STARTED |
	//		EVENT_PLAY_STOPPED | EVENT_PLAY_EOF);*/

	//	const char* url = "D:\\≤‚ ‘\\test.mp4";
	//	//LOG_INFO("Starting playback: %s", url);
	//	//LOG_INFO("Playback finished. Press ESC or close window to exit.");
	//	int running = 1;
	//	int ret = play_start(play, url);
	//}

	//memset(&play, 0, sizeof(Play));

	//static co_mutex _mtx;
	//static bool done = false;
	//for (int i = 0; i < 192; i++) {
	//	go[]() {
	//	auto d=	av_gettime_relative();
	//		_mtx.lock();
	//		if (!done) {
	//			Sleep(8000);
	//			done = true;

	//		}	
	//		_mtx.unlock();
	//		d = av_gettime_relative() - d;
	//		printf("enter lock cost %lldus\n", d);
	//	};
	//}



	//SDL_Delay(2000000);
	Play* play = new Play;
	play->onDisplay +=[](auto s, auto e) {
		AVFrame* frame = e->frame;
		if (!g_window) {
			int w = frame->width;
			int h = frame->height;
			g_window = SDL_CreateWindow("FFmpeg Player Test",
				SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
				w, h, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
			g_renderer = SDL_CreateRenderer(g_window, -1,
				SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/);
			g_texture = SDL_CreateTexture(g_renderer,
				SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STATIC, w, h);
		}
		if (g_texture && g_renderer) {
			SDL_UpdateYUVTexture(g_texture, NULL,
				frame->data[0], frame->linesize[0],
				frame->data[1], frame->linesize[1],
				frame->data[2], frame->linesize[2]);
			SDL_RenderClear(g_renderer);
			SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
			SDL_RenderPresent(g_renderer);
			SDL_Event event;
			SDL_PollEvent(&event);
		}
		};
	/*for (int i = 0; i < 192; i++) {
		Play* play = new Play;
		play->start("C:\\test.mp4");
	}
	SDL_Delay(2000000);*/

	//Play* play = new Play;

	//play->start("C:\\test.mp4");
	//play->start("http://69.30.245.50/live/cctv1.m3u8");
	//play->start("http://198.204.240.250:82/live/cctv1.m3u8");
	//play->start("http://112.123.243.37:50085/tsfile/live/1002_1.m3u8?key=txiptv&playlive=0&authid=0");


	play->start("http://173.208.212.130:8181/720p/hnws.m3u8");

	////play->start("D:\\≤‚ ‘\\cross_road.mp4");
	//play->start("rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid");
	//play->start("rtsp://172.20.253.38:2554/param?src=RTSP://admin:avcit12345678@192.168.1.201:554/h264/ch33/main/av_stream");



	//while (1) {
	//	play->start("C:\\test.mp4");
	//	SDL_Delay(2000);
	//	play->stop();
	//}
	SDL_Delay(2000000);
	return 0;
}