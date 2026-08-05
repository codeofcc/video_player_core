// video_player_core.cppm
module;

#include "Play.h"

export module video_player_core;  // ✅ 模块名也改成 video_player_core

export class VideoPlayerCore : public ::Play {
public:
    using ::Play::Play;
};