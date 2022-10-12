/*
*  Copyright (c) 2016 Stefan Sincak. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree.
*/

#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include "webm_player.hpp"

#include <SDL.h>
#include <iostream>
#undef Success

#include "build/dxxsconf.h"
#ifdef DXX_USE_SDLMIXER
#include <SDL_mixer.h>
#endif
namespace dcx
{
	SDL_Surface *gr_get_sdl_screen(void);
}


namespace wp
{
	// called by libWebmPlayer when new PCM data is available
	static void OnAudioData(void *userPtr, float *pcm, size_t count)
	{
		WebmPlayer *player = static_cast<WebmPlayer*>(userPtr);

		player->insertAudioData(pcm, count);
	}

	// called by SDL when audio device wants PCM data
	static void sdlAudioCallback(void *userPtr, Uint8 *stream, int len)
	{
		WebmPlayer *player = static_cast<WebmPlayer*>(userPtr);
		#ifdef SDL2
		player->readAudioData((float*)stream, len / sizeof(float));
		#else
		float buf[4096];
		int samples = len / 2;
		int16_t *dst = reinterpret_cast<int16_t *>(stream);

		if (samples > 4096)
			abort();

		player->readAudioData(buf, samples);
		for (int i = 0; i < samples; i++)
			dst[i] = buf[i] * 32767;
		#endif
	}

	static uint8_t bclamp(int n)
	{
		return n < 0 ? 0 : n > 255 ? 255 : n;
	}

	__attribute__((optimize("-O3")))
	static void yuvToRgb(uint8_t *ybuf, uint8_t *ubuf, uint8_t *vbuf, int ypitch, int uvpitch,
		uint8_t *rgb, int w, int h)
	{
		h >>= 1;
		while (h--) {
			uint8_t *rgb2 = rgb + w * 3, *yp = ybuf, *up = ubuf, *vp = vbuf;
			for (int i = w >> 1; i--; ) {
				int u = *up++ - 128, v = *vp++ - 128;
				int r = v * 419 / 299;
				int g = v * -299 / 419 + u * -114 / 331;
				int b = u * 587 / 331;
				int y1 = yp[0], y2 = yp[1];
				//y1 += y1 >> 4; y2 += y2 >> 4;
				rgb[0] = bclamp(y1 + r); rgb[1] = bclamp(y1 + g); rgb[2] = bclamp(y1 + b);
				rgb[3] = bclamp(y2 + r); rgb[4] = bclamp(y2 + g); rgb[5] = bclamp(y2 + b);
				y1 = yp[ypitch]; y2 = yp[ypitch + 1];
				//y1 += y1 >> 4; y2 += y2 >> 4;
				rgb2[0] = bclamp(y1 + r); rgb2[1] = bclamp(y1 + g); rgb2[2] = bclamp(y1 + b);
				rgb2[3] = bclamp(y2 + r); rgb2[4] = bclamp(y2 + g); rgb2[5] = bclamp(y2 + b);
				yp += 2;
				rgb += 6;
				rgb2 += 6;
			}
			rgb = rgb2;
			ybuf += ypitch * 2;
			ubuf += uvpitch;
			vbuf += uvpitch;
		}
	}

	WebmPlayer::WebmPlayer()
	{
	}

	WebmPlayer::~WebmPlayer()
	{
		destroy();
	}

	bool  WebmPlayer::load(const char *fileName)
	{
		return load(SDL_RWFromFile(fileName, "rb"));
	}

	bool  WebmPlayer::load(SDL_RWops *file)
	{
		if (!loadVideo(file))
			return false;

		if (!initSDL(NULL, m_video->info().width, m_video->info().height))
			return false;

		if (!initAudio())
			return false;

		// print some video stats
		//printInfo();

		return true;
	}

	bool WebmPlayer::loadVideo(SDL_RWops *file)
	{
		m_video = new uvpx::Player(uvpx::Player::defaultConfig());

		uvpx::Player::LoadResult res = m_video->load(file, 0, false);

		switch (res)
		{
		case uvpx::Player::LoadResult::FileNotExists:
			log("Failed to open video file");
			return false;

		case uvpx::Player::LoadResult::UnsupportedVideoCodec:
			log("Unsupported video codec");
			return false;

		case uvpx::Player::LoadResult::NotInitialized:
			log("Video player not initialized");
			return false;

		case uvpx::Player::LoadResult::Success:
			log("Video loaded successfully");
			break;

		default:
			log("Failed to load video (%d)", res);
			return false;
		}

		m_video->setOnAudioData(OnAudioData, this);

		return true;
	}

	bool WebmPlayer::initSDL(const char *windowTitle, int width, int height)
	{
		#if 1
		(void)windowTitle;
		m_width = width;
		m_height = height;
		m_rgb = new uint8_t[width * height * 3];
		#else
		#ifdef SDL2
		if (SDL_Init(
			SDL_INIT_TIMER |
			SDL_INIT_AUDIO |
			SDL_INIT_VIDEO |
			SDL_INIT_EVENTS) != 0)
		{
			log("Failed to initialize SDL (%s)", SDL_GetError());
			return false;
		}

		// create window
		m_window = SDL_CreateWindow(
			windowTitle,
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			width,
			height,
			SDL_WINDOW_SHOWN | SDL_WINDOW_MINIMIZED);
		if (!m_window)
		{
			log("Could not create SDL window (%s)", SDL_GetError());
			return false;
		}

		// create renderer
		m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
		if (!m_renderer)
		{
			log("Could not create SDL renderer (%s)", SDL_GetError());
			return false;
		}

		// create video texture
		m_texture = SDL_CreateTexture(
			m_renderer,
			SDL_PIXELFORMAT_YV12,
			SDL_TEXTUREACCESS_STREAMING,
			width,
			height
			);
		if (!m_texture)
		{
			log("Could not create SDL video texture (%s)", SDL_GetError());
			return false;
		}
		#else
		(void)windowTitle;
		#if 1
		SDL_Surface *screen = SDL_SetVideoMode(width, height, 0, 0);
		#else
		SDL_Surface *screen = dcx::gr_get_sdl_screen();
		#endif
		m_overlay = SDL_CreateYUVOverlay(width, height, SDL_YV12_OVERLAY, screen);
		m_width = width;
		m_height = height;
		#endif
		#endif

		return true;
	}

	#if 0
	static void planecpy(uint8_t *dst, uint8_t *src, int dst_pitch, int src_pitch, int w, int h) {
		while (h--) {
			memcpy(dst, src, w);
			dst += dst_pitch;
			src += src_pitch;
		}
	}
	#endif

	void WebmPlayer::updateVideo()
	{
		uvpx::Frame *yuv = nullptr;

		if (!m_video)
			return;

		Uint32 time = SDL_GetTicks();
		float dt = m_last_time ? (time - m_last_time) / 1000.0f : 0;
		m_last_time = time;

		// update video
		m_video->update(dt);

		// check if we have new YUV frame
		if ((yuv = m_video->lockRead()) != nullptr)
		{
			#if 1
			yuvToRgb(yuv->y(), yuv->u(), yuv->v(), yuv->yPitch(), yuv->uvPitch(), m_rgb, m_width, m_height);
			m_video->unlockRead();
			if (m_video_cb)
				m_video_cb(m_rgb, m_width, m_height, m_video_cb_data);
			#else
			#ifdef SDL2
			SDL_UpdateYUVTexture(
				m_texture,
				NULL,
				yuv->y(),
				yuv->yPitch(),
				yuv->u(),
				yuv->uvPitch(),
				yuv->v(),
				yuv->uvPitch()
				);

			m_video->unlockRead();

			SDL_RenderClear(m_renderer);
			SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
			SDL_RenderPresent(m_renderer);
			#else
			SDL_LockYUVOverlay(m_overlay);
			planecpy(m_overlay->pixels[0], yuv->y(), m_overlay->pitches[0], yuv->yPitch(),
				m_width, m_height);
			planecpy(m_overlay->pixels[1], yuv->v(), m_overlay->pitches[1], yuv->uvPitch(),
				m_width / 2, m_height / 2);
			planecpy(m_overlay->pixels[2], yuv->u(), m_overlay->pitches[2], yuv->uvPitch(),
				m_width / 2, m_height / 2);
			SDL_UnlockYUVOverlay(m_overlay);
			m_video->unlockRead();
			SDL_Rect rect;
			rect.x = rect.y = 0;
			rect.w = m_width;
			rect.h = m_height;
			SDL_DisplayYUVOverlay(m_overlay, &rect);
			#endif
			#endif
		} else // redraw last frame
			if (m_video_cb)
				m_video_cb(m_rgb, m_width, m_height, m_video_cb_data);

		#if 0
		// calc elapsed time
		Uint32 endTime = SDL_GetTicks();
		dt = (endTime - startTime) / 1000.0f;
		startTime = endTime;
		#endif
	}

	void  WebmPlayer::run()
	{
		if (m_video == nullptr)
			return;

		// start playing immediatelly
		playback(Play);

		bool running = true;

		//Uint32 startTime = SDL_GetTicks();
		//float dt = 1.0f / 60.0f;

		while (running)
		{
			// handle SDL events
			SDL_Event evt;
			while (SDL_PollEvent(&evt))
			{
				switch (evt.type)
				{
				case SDL_QUIT:
					running = false;
					break;

				case SDL_KEYDOWN:
				{
					switch (evt.key.keysym.sym)
					{
					case SDLK_ESCAPE:
						running = false;
						break;

					case SDLK_SPACE:
						if (m_video->isPlaying())
							playback(Pause);
						else if (m_video->isPaused())
							playback(Resume);
						else if (m_video->isStopped())
							playback(Play);

						break;

					case SDLK_s:
						playback(Stop);
						break;
					default:
						break;
					}

					break;
				}

				default:
					break;
				}
			}
			updateVideo();
		}
	}

	void  WebmPlayer::destroy()
	{
		if (m_video)
			delete m_video;

		#ifdef SDL2
		if(m_audioDevice != 0)
		{
			SDL_CloseAudioDevice(m_audioDevice);
			m_audioDevice = 0;
		}

		if (m_texture)
		{
			SDL_DestroyTexture(m_texture);
			m_texture = nullptr;
		}

		if (m_renderer)
		{
			SDL_DestroyRenderer(m_renderer);
			m_renderer = nullptr;
		}

		if (m_window)
		{
			SDL_DestroyWindow(m_window);
			m_window = nullptr;
		}
		#endif

		#ifdef DXX_USE_SDLMIXER
		Mix_SetPostMix(NULL, NULL);
		#endif

		#if 1
		if (m_rgb) {
			delete[] m_rgb;
			m_rgb = nullptr;
		}
		#else
		SDL_Quit();
		#endif
	}

	void  WebmPlayer::log(const char *format, ...)
	{
		using namespace std;

		char buffer[512];

		va_list arglist;
		va_start(arglist, format);
		vsprintf(buffer, format, arglist);
		va_end(arglist);

		cout << buffer << endl;
	}

	void  WebmPlayer::printInfo()
	{
		uvpx::Player::VideoInfo info = m_video->info();

		log("Clip info:\n"
			"- width = %d\n"
			"- height = %d\n"
			"- duration = %f (sec.)\n"
			"- has audio = %s\n"
			"- audio channels = %d\n"
			"- audio frequency = %d\n"
			"- audio samples = %d\n"
			"- decode threads count = %d\n"
			"- frame rate = %f\n",
			info.width,
			info.height,
			info.duration,
			info.hasAudio ? "yes" : "no",
			info.audioChannels,
			info.audioFrequency,
			info.audioSamples,
			info.decodeThreadsCount,
			info.frameRate);
	}

	bool WebmPlayer::initAudio()
	{
		uvpx::Player::VideoInfo info = m_video->info();

		if (!info.hasAudio)
			return true;

		SDL_AudioSpec wanted_spec, obtained_spec;

		wanted_spec.freq = info.audioFrequency;
		#ifdef SDL2
		wanted_spec.format = AUDIO_F32;
		#else
		wanted_spec.format = AUDIO_S16;
		#endif
		wanted_spec.channels = info.audioChannels;
		wanted_spec.silence = 0;
		wanted_spec.samples = 4096;
		wanted_spec.callback = sdlAudioCallback;
		wanted_spec.userdata = this;

		#ifdef SDL2
		m_audioDevice = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &obtained_spec, 0);
		if (m_audioDevice == 0)
		#else
		#ifdef DXX_USE_SDLMIXER
		Mix_SetPostMix(wanted_spec.callback, wanted_spec.userdata);
		obtained_spec = wanted_spec;
		if (0)
		#else
		if (SDL_OpenAudio(&wanted_spec, &obtained_spec) < 0)
		#endif
		#endif
		{
			log("Could not create SDL audio (%s).", SDL_GetError());
			return false;
		}
		else if (wanted_spec.format != obtained_spec.format)
		{
			log("Wanted and obtained SDL audio formats are different! (%d : %d)", wanted_spec.format, obtained_spec.format);
			return false;
		}

		#ifdef SDL2
		SDL_PauseAudioDevice(m_audioDevice, 0);
		#else
		SDL_PauseAudio(0);
		#endif
		
		return true;
	}

	void  WebmPlayer::insertAudioData(float *src, size_t count)
	{
		std::lock_guard<std::mutex> lock(m_audioMutex);

		m_audioBuffer.resize(m_audioBuffer.size() + count);
		memcpy(&m_audioBuffer[m_audioBuffer.size() - count], src, count * sizeof(float));
	}

	bool  WebmPlayer::readAudioData(float *dst, size_t count)
	{
		std::lock_guard<std::mutex> lock(m_audioMutex);

		if (m_audioBuffer.size() < count) {
			memset(dst, 0, count * sizeof(float));
			return false;
		}

		memcpy(dst, &m_audioBuffer[0], count * sizeof(float));

		m_audioBuffer.erase(m_audioBuffer.begin(), m_audioBuffer.begin() + count);

		return true;
	}

	void  WebmPlayer::clearAudioData()
	{
		std::lock_guard<std::mutex> lock(m_audioMutex);
		m_audioBuffer.clear();
	}

	void  WebmPlayer::playback(PlaybackCommand cmd)
	{
		switch (cmd)
		{
		case PlaybackCommand::Play:
			m_last_time = 0;
			m_video->play();
			#ifdef SDL2
			SDL_PauseAudioDevice(m_audioDevice, 0);
			#else
			SDL_PauseAudio(0);
			#endif
			break;

		case PlaybackCommand::Pause:
			m_video->pause();
			#ifdef SDL2
			SDL_PauseAudioDevice(m_audioDevice, 1);
			#else
			SDL_PauseAudio(1);
			#endif
			break;

		case PlaybackCommand::Resume:
			m_video->play();
			#ifdef SDL2
			SDL_PauseAudioDevice(m_audioDevice, 0);
			#else
			SDL_PauseAudio(0);
			#endif
			break;

		case PlaybackCommand::Stop:
			m_video->stop();
			#ifdef SDL2
			SDL_PauseAudioDevice(m_audioDevice, 1);
			SDL_ClearQueuedAudio(m_audioDevice);
			#else
			SDL_PauseAudio(1);
			#endif
			clearAudioData();
			break;
		}
	}
}
