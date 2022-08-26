/*
*  Copyright (c) 2016 Stefan Sincak. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree.
*/

#ifndef _WEBM_PLAYER_HPP_
#define _WEBM_PLAYER_HPP_

#include <SDL.h>
#include "player.hpp"

#include <vector>
#include <mutex>

namespace wp
{
	typedef void (video_callback_t)(const uint8_t *rgb, int w, int h, void *data);

    class WebmPlayer
    {
    public:
        enum PlaybackCommand
        {
            Play,
            Pause,
            Stop,
            Resume
        };

    public:
        WebmPlayer();
        virtual ~WebmPlayer();

        bool  load(const char *fileName);
        bool  load(SDL_RWops *file);
        void  run();
        void  destroy();
        void  updateVideo();

        void  playback(PlaybackCommand cmd);

        void  insertAudioData(float *src, size_t count);
        bool  readAudioData(float *dst, size_t count);
        void  clearAudioData();

        inline void  setVideoCallback(video_callback_t *cb, void *data) { m_video_cb = cb; m_video_cb_data = data; }
        inline int  width() { return m_video ? m_video->info().width : 0; }
        inline int  height() { return m_video ? m_video->info().height : 0; }
        inline bool  finished() { return m_video ? m_video->finished() : false; }

    private:
        uvpx::Player  *m_video = nullptr;

        #if 1
        int m_width = 0, m_height = 0;
        uint8_t *m_rgb = nullptr;
        video_callback_t *m_video_cb = nullptr;
        void *m_video_cb_data = nullptr;
        Uint32 m_last_time = 0;
        #else
        #ifdef SDL2
        SDL_Window  *m_window = nullptr;
        SDL_Renderer  *m_renderer = nullptr;
        SDL_Texture  *m_texture = nullptr;
        SDL_AudioDeviceID  m_audioDevice = 0;
        #else
        SDL_Overlay  *m_overlay = NULL;
        int  m_width = 0;
        int  m_height = 0;
        #endif
        #endif

        void log(const char *format, ...);
        void printInfo();

        bool loadVideo(SDL_RWops *file);
        bool initSDL(const char *windowTitle, int width, int height);

        bool initAudio();

        std::vector<float> m_audioBuffer;
        std::mutex  m_audioMutex;
    };
}

#endif
