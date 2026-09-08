// video_player_core.cppm
module;

#include "play.h"
#include "models.h"

export module video_player_core;  // ✅ 模块名也改成 video_player_core

// Export necessary types for player.cpp
export using ::Play;
export using ::DisplayEventArgs;
export using ::CursorTimeChangedEventArgs;

export class VideoPlayerCore : public ::Play {
public:
    using ::Play::Play;
};