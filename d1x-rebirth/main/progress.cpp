#include <math.h>
#include "newmenu.h"
#include "pcx.h"
#include "dxxerror.h"
#include "window.h"
#include "ogl_init.h"
#include "timer.h"
#include "gamefont.h"
#include "screens.h"
#include "key.h"
#include "config.h"
#include "gameseq.h"
#include "physfsrwops.h"
#include "game.h"
#include "args.h"

namespace d1x {
int play_webm_movie(const char *const filename);

#define BMS_MAP 0
#define BMS_SHIP 3
#define BMS_SHIP_NUM 2
#define BMS_NUM 5

static const char *bms_files[] = { "map01.pcx", "map02.pcx", "map03.pcx", "ship01.pcx", "ship02.pcx" };

struct solarmap final : ::dcx::window
{
	using ::dcx::window::window;
	palette_array_t pals[5];
	grs_main_bitmap	bms[5];
	//grs_main_bitmap ships[2];
	//fix ship_x, ship_y, ship_dx, ship_dy;
	fix64 start_time;
	float step;
	int ship_idx;
	int next_idx;
	int end_idx;
	int reversed;
	int ending;
	int done;
	int select;
	fix64 last_frame;
	fix frame_time;
	solarmap(grs_canvas &src, int x, int y, int w, int h);
	virtual window_event_result event_handler(const d_event &) override;
	void move(int dir);
};

#define NUM_LVLINFO 34
struct {
	short level, map, x, y;
	const char *name;
} lvlinfo[NUM_LVLINFO] = {
	{ 1, 0, 286, 143, "Lunar Outpost" },
	{ 2, 0, 249, 136, "Lunar Scilab" },
	{ 3, 0, 208, 120, "Lunar Military Base" },
	{ 4, 0, 157, 110, "Venus Atmosphere Lab" },
	{ 5, 0, 130, 115, "Venus Nickel-Iron Mine" },
	{ 6, 0, 97, 140, "Mercury Solar Lab" },
	{ 7, 0, 82, 168, "Mercury Military Lab" },
	{ -100, 0, 70, 199, "None1" },
	{ -101, 1, 0, 125, "None2" },
	{ 8, 1, 26, 105, "Mars Processing Station" },
	{ 9, 1, 53, 72, "Mars Colony" },
	{ 10, 1, 83, 64, "Mars Orbital Station" },
	{ -1, 1, 94, 69, "Secret Level #1" },
	{ 11, 1, 106, 74, "Io Sulpher Mine" },
	{ 12, 1, 125, 100, "Callisto Tower Colony" },
	{ 13, 1, 145, 112, "Europa Mining Colony" },
	{ 14, 1, 177, 119, "Ganymede Military Base" },
	{ 15, 1, 205, 135, "Europa Diamond Mine" },
	{ 16, 1, 225, 159, "Hyperion Methane Mine" },
	{ 17, 1, 262, 182, "Tethys Military Base" },
	{ -102, 1, 319, 193, "None3" },
	{ -103, 2, 0, 27, "None4" },
	{ 18, 2, 15, 48, "Miranda Mine" },
	{ 19, 2, 19, 76, "Oberon Mine" },
	{ 20, 2, 25, 110, "Oberon Iron Mine" },
	{ 21, 2, 65, 131, "Oberon Colony" },
	{ -2, 2, 86, 125, "Secret Level #2" },
	{ 22, 2, 107, 120, "Neptune Storage Depot" },
	{ 23, 2, 144, 96, "Triton Storage Depot" },
	{ 24, 2, 188, 90, "Nereid Volatile Mine" },
	{ -3, 2, 211, 96, "Secret Level #3" },
	{ 25, 2, 235, 102, "Pluto Outpost" },
	{ 26, 2, 267, 108, "Pluto Military Base" },
	{ 27, 2, 296, 98, "Charon Volatile Mine" } };

static void show_bitmap_at(grs_canvas &canvas, grs_bitmap &bmp, int x, int y, int w, int h)
{
	const bool hiresmode = 0; //HIRESMODE;
	const auto sw = static_cast<float>(SWIDTH) / (hiresmode ? 640 : 320);
	const auto sh = static_cast<float>(SHEIGHT) / (hiresmode ? 480 : 240);
	const float scale = (sw < sh) ? sw : sh;

	#if 1
	ogl_ubitmapm_cs(canvas, x * scale, y * scale, (w ? w : bmp.bm_w)*scale, (h ? h : bmp.bm_h)*scale, bmp, ogl_colors::white);
	#else
	int org_h = bmp.bm_h, org_w = bmp.bm_w;
	const color_palette_index *org_data = bmp.bm_data;

	if (w && x + w > 320)
		bmp.bm_w = w = 320 - x;
	if (h && y + h > 200)
		bmp.bm_h = h = 200 - y;
	if (w && x < 0) {
		gr_set_bitmap_data(bmp, org_data + -x);
		bmp.bm_w = w = w + x;
		x = 0;
	}
	//printf("%d,%d,%d,%d\n", x, y, x + w, y + h);
	auto bitmap_canv = gr_create_sub_canvas(canvas, x*scale, y*scale, (w ? w : bmp.bm_w)*scale, (h ? h : bmp.bm_h)*scale);
	show_fullscr(*bitmap_canv, bmp);
	bmp.bm_w = org_w;
	bmp.bm_h = org_h;
	gr_set_bitmap_data(bmp, org_data);
	//ogl_ubitmapm_cs(canvas, x*scale, y*scale, (w ? w : bmp.bm_w)*scale, (h ? h : bmp.bm_h)*scale, bmp, ogl_colors::white); 
	#endif
}

#if 0
static void set_speed(solarmap &sm, int idx)
{
	int dx = lvlinfo[idx + 1].x - lvlinfo[idx].x;
	int dy = lvlinfo[idx + 1].y - lvlinfo[idx].y;
	float dist = sqrtf(dx * dx + dy * dy);
	sm.speed = 1 / dist;
}
#endif

static void draw_solarmap(solarmap &sm)
{
	const auto w = static_cast<float>(SWIDTH) / 320;
	const auto h = static_cast<float>(SHEIGHT) / 240;
	const float scale = (w < h) ? w : h;
	auto canv = gr_create_sub_canvas(*grd_curcanv, 0, 20*scale, 320*scale, 200*scale);
	int cur_map = lvlinfo[sm.ship_idx].map;
	fix64 time = timer_query() - sm.start_time;
	bool hide_cur = !sm.select && !sm.ending && time < F2_0 && (time & (1 << 13));

	gr_clear_canvas(*grd_curcanv, BM_XRGB(0,0,0));

	gr_palette_load(gr_palette = sm.pals[BMS_MAP + cur_map]);

	show_fullscr(*canv, sm.bms[BMS_MAP + cur_map]);

	gr_palette[1].r = gr_palette[1].b = 0;
	gr_palette[1].g = 8;
	gr_palette[2].r = gr_palette[2].b = 0;
	gr_palette[2].g = 14;
	gr_palette[3].r = gr_palette[3].b = 0;
	gr_palette[3].g = 28;
	gr_palette_load(gr_palette);
	color_palette_index c1 = 1; //BM_XRGB(0, 4, 0);
	color_palette_index c2 = 2; //BM_XRGB(0, 7, 0);
	color_palette_index c3 = 3; //BM_XRGB(0, 14, 0);
	for (int i = 0; i < NUM_LVLINFO - 1 && i < sm.end_idx; i++) {
		if (lvlinfo[i].map != cur_map || lvlinfo[i + 1].map != cur_map)
			continue;
		if (hide_cur && i == sm.ship_idx) {
			gr_disk(*canv, i2f(lvlinfo[i].x * scale), i2f(lvlinfo[i].y * scale), fl2f(scale), c3);
			continue;
		}
		int x1 = lvlinfo[i].x * scale, y1 = lvlinfo[i].y * scale, x2 = lvlinfo[i + 1].x * scale, y2 = lvlinfo[i + 1].y * scale;
		for (int j = scale * 2; j > 0; j--) {
			int c = j >= scale ? c1 : c2;
			gr_line(*canv, i2f(x1), i2f(y1 - j), i2f(x2), i2f(y2 - j), c);
			gr_line(*canv, i2f(x1 + j), i2f(y1), i2f(x2 + j), i2f(y2), c);
			gr_line(*canv, i2f(x1), i2f(y1 + j), i2f(x2), i2f(y2 + j), c);
			gr_line(*canv, i2f(x1 - j), i2f(y1), i2f(x2 - j), i2f(y2), c);
		}
		gr_line(*canv, i2f(x1), i2f(y1), i2f(x2), i2f(y2), c2);
		if (lvlinfo[i].level > -100) {
			gr_disk(*canv, i2f(x1), i2f(y1), fl2f(scale * 2), c2);
			gr_disk(*canv, i2f(x1), i2f(y1), fl2f(scale), c3);
			//gr_disk(*canv, i2f(x1), i2f(y1), fl2f(scale / 2), c3);
		}

		if (lvlinfo[i + 1].level > -100) {
			gr_disk(*canv, i2f(x2), i2f(y2), fl2f(scale * 2), c2);
			//gr_disk(*canv, i2f(x2), i2f(y2), fl2f(scale), c3);
		}
	}

	//int next_idx = sm.ship_idx + 1;
	//while (lvlinfo[next_idx].level <= -100)
	//	next_idx++;
	int next_idx = sm.next_idx;
	if (sm.select) {
		int idx = sm.ship_idx;
		if (lvlinfo[idx].level < 0) // secret or transition
			idx += sm.reversed ? -1 : 1;
		gr_string(*canv, *MEDIUM3_FONT, 0x8000, 4 * scale, "Select starting level");
		gr_string(*canv, *MEDIUM1_FONT, 0x8000, (4 + 2) * scale + FNTScaleY * MEDIUM1_FONT->ft_h,
			lvlinfo[idx].name);
	} else {
		gr_string(*canv, *MEDIUM1_FONT, 0x8000, 4 * scale, "Progressing to");
		gr_string(*canv, *MEDIUM1_FONT, 0x8000, (4 + 2) * scale + FNTScaleY * MEDIUM1_FONT->ft_h,
			lvlinfo[next_idx].name);
	}

	//int x = fixmul(lvlinfo[sm.ship_idx].x, F1_0 - sm.step) + fixmul(lvlinfo[sm.ship_idx + 1].x, sm.step);
	//int y = fixmul(lvlinfo[sm.ship_idx].y, F1_0 - sm.step) + fixmul(lvlinfo[sm.ship_idx + 1].y, sm.step);
	int dir = sm.reversed ? -1 : 1;
	int x = lvlinfo[sm.ship_idx].x * (1 - sm.step) + lvlinfo[sm.ship_idx + dir].x * sm.step;
	int y = lvlinfo[sm.ship_idx].y * (1 - sm.step) + lvlinfo[sm.ship_idx + dir].y * sm.step;

	int ship_type = (lvlinfo[sm.ship_idx].map != 0) ^ sm.reversed;
	gr_palette_load(gr_palette = sm.pals[BMS_SHIP + ship_type]);
	show_bitmap_at(*canv, sm.bms[BMS_SHIP + ship_type], x - 24, y - 13, 48, 27);
	
	if (sm.ship_idx == sm.next_idx) {
		if (!sm.ending) {
			sm.start_time = timer_query();
			sm.ending = 1;
		} else if (time > F2_0 && !sm.select)
			sm.done = 1;
	} else if (time > F2_0 || sm.select) {
		if (sm.step < 1) {
			int dx = lvlinfo[sm.ship_idx + dir].x - lvlinfo[sm.ship_idx].x;
			int dy = lvlinfo[sm.ship_idx + dir].y - lvlinfo[sm.ship_idx].y;
			float dist = sqrtf(dx * dx + dy * dy);

			sm.step += 1.0f / dist * f2fl(sm.frame_time) * 100;
		}
		if (sm.step >= 1 && (sm.reversed ? sm.ship_idx > 0 : sm.ship_idx + 1 < NUM_LVLINFO)) {
			sm.step -= 1;
			//do {
				sm.ship_idx += dir;
			//} while (sm.ship_idx != sm.next_idx && lvlinfo[sm.ship_idx].level <= -100);
			if (lvlinfo[sm.ship_idx].level <= -100) // map transition
				sm.ship_idx += dir;
			if (sm.ship_idx == sm.next_idx) {
				int left = lvlinfo[sm.ship_idx].map ? -1 : 1;
				if (keyd_pressed[KEY_LEFT] || keyd_pressed[KEY_PAD4])
					sm.move(left);
				else if (keyd_pressed[KEY_RIGHT] || keyd_pressed[KEY_PAD6])
					sm.move(-left);
				sm.step = 0;
			}
			sm.start_time = timer_query();
		} else if (sm.step >= 1)
			sm.step = 1;
	}
}

solarmap::solarmap(grs_canvas &src, int x, int y, int w, int h) :
	window(src, x, y, w, h),
	end_idx(NUM_LVLINFO),
	reversed(0),
	ending(0), done(0), select(0)
{
	last_frame = timer_query();
	frame_time = FrameTime;
}

static void do_delay(fix64 &t1, fix &frame_time) {
	fix64 t2 = timer_query();
	const auto vsync = CGameCfg.VSync;
	const auto bound = F1_0 / (vsync ? MAXIMUM_FPS : CGameArg.SysMaxFPS);
	const auto may_sleep = !CGameArg.SysNoNiceFPS && !vsync;
	while (t2 - t1 < bound) // ogl is fast enough that the automap can read the input too fast and you start to turn really slow.  So delay a bit (and free up some cpu :)
	{
		//if (Game_mode & GM_MULTI)
		//	multi_do_frame(); // during long wait, keep packets flowing
		if (may_sleep)
			timer_delay(F1_0>>8);
		t2 = timer_update();
	}
	frame_time=t2-t1;
	t1 = t2;
}

void solarmap::move(int dir)
{
	if (dir && this->select && this->ship_idx == this->next_idx) {
		int i = this->ship_idx;
		for (;;) {
			i += dir;
			if (i < 0 || i >= NUM_LVLINFO)
				break;
			if (lvlinfo[i].level > 0) {
				this->next_idx = i;
				this->reversed = dir < 0;
				break;
			}
		}
	}
}

window_event_result solarmap::event_handler(const d_event &event)
{
	switch (event.type)
	{
		case EVENT_WINDOW_DRAW:
			draw_solarmap(*this);
			if (this->done)
				return window_event_result::close;
			do_delay(this->last_frame, this->frame_time);
			break;
			
		case EVENT_KEY_COMMAND: {
			int key = event_key_get(event);
			int left = lvlinfo[this->ship_idx].map ? -1 : 1;
			if (key == KEY_ESC) {
				return window_event_result::close;
			} else if (key == KEY_ENTER || key == KEY_PADENTER) {
				this->done = 1;
				return window_event_result::close;
			} else if (key == KEY_LEFT || key == KEY_PAD4) {
				this->move(left);
			} else if (key == KEY_RIGHT || key == KEY_PAD6) {
				this->move(-left);
			}
			break;
		}

		case EVENT_WINDOW_CLOSE:
			this->~window();
			return window_event_result::deleted;	// skip deletion

		default:
			return window_event_result::ignored;
			break;
	}
	return window_event_result::handled;
}

#if 0
static void patch_pal(palette_array_t &pal) {
	pal[254].r = pal[254].b = 0;
	pal[254].g = 8 + 2;
	pal[253].r = pal[253].b = 0;
	pal[253].g = 14 + 4;
}
#endif

int DoProgressing(int next_level)
{
	#if 0
	play_webm_movie("d1intro.webm");
	event_process_all();
	printf("movie done\n");
	#endif

	//palette_array_t pal;
	auto sm = window_create<solarmap>(grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT);
	//char filename[64];
	
	for (int i = 0; i < BMS_NUM; i++) {
		sm->bms[i].reset();
		const auto pcx_error = pcx_read_bitmap_or_default(bms_files[i], sm->bms[i], sm->pals[i]);
		if (pcx_error != pcx_result::SUCCESS) {
			con_printf(CON_URGENT, DXX_STRINGIZE_FL(__FILE__, __LINE__, "solarmap: File %s - PCX error: %s"), bms_files[i], pcx_errormsg(pcx_error));
			gr_init_bitmap_alloc(sm->bms[i], bm_mode::linear, 0, 0, 2, 2, 2);
			memset(sm->bms[i].get_bitmap_data(), 0, 2 * 2);
		}
		if (i >= BMS_SHIP && i < BMS_SHIP + BMS_SHIP_NUM) {
			gr_palette_load(gr_palette = sm->pals[i]);
			gr_remap_bitmap_good(sm->bms[i], sm->pals[i], 254, -1); // fix transparent color
		}
	}

	#if 0
	//nm_messagebox(menu_title{nullptr}, 1, "Ok", "hello");
	strcpy(filename, "map01.pcx");
	const auto pcx_error = pcx_read_bitmap_or_default(filename, sm->maps[0], sm->mapspal[0]);
	if (pcx_error != pcx_result::SUCCESS)
		con_printf(CON_URGENT, DXX_STRINGIZE_FL(__FILE__, __LINE__, "solarmap: File %s - PCX error: %s"), filename, pcx_errormsg(pcx_error));
	sm->maps[1].reset();
	pcx_read_bitmap_or_default("map02.pcx", sm->maps[1], sm->mapspal[1]);
	sm->maps[2].reset();
	pcx_read_bitmap_or_default("map03.pcx", sm->maps[2], sm->mapspal[2]);
	
	for (int i = 0; i < 3; i++) {
		gr_palette = sm->mapspal[i];
		patch_pal(gr_palette);
		gr_palette_load(gr_palette);
		gr_remap_bitmap_good(sm->maps[i], sm->mapspal[i], TRANSPARENCY_COLOR, -1);
		gr_set_transparent(sm->maps[i], 0);
		patch_pal(sm->mapspal[i]);
	}

	sm->ships[0].reset();
	pcx_read_bitmap_or_default("ship01.pcx", sm->ships[0], pal);
	gr_palette = pal;
	patch_pal(gr_palette);
	gr_palette_load(gr_palette);
	gr_remap_bitmap_good(sm->ships[0], pal, 254, -1);
	
	sm->ships[1].reset();
	pcx_read_bitmap_or_default("ship02.pcx", sm->ships[1], pal);
	gr_remap_bitmap_good(sm->ships[1], pal, 254, -1);
	#endif

	#if 0
	sm->ship_x = i2f(lvlinfo[0].x);
	sm->ship_y = i2f(lvlinfo[0].y);
	sm->ship_dx = (i2f(lvlinfo[1].x) - i2f(lvlinfo[0].x)) / 100;
	sm->ship_dy = (i2f(lvlinfo[1].y) - i2f(lvlinfo[0].y)) / 100;
	#endif
	sm->step = 0;

	if (next_level == -100) { // select
		sm->select = 1;
		sm->next_idx = sm->ship_idx = 0;
		sm->end_idx = NUM_LVLINFO - 1;
	} else { // progress
		for (int i = 0; i < NUM_LVLINFO; i++)
			if (lvlinfo[i].level == Current_level_num)
				sm->ship_idx = i;
		sm->end_idx = sm->next_idx = sm->ship_idx;
		for (int i = 0; i < NUM_LVLINFO; i++)
			if (lvlinfo[i].level == next_level)
				sm->end_idx = sm->next_idx = i;
	}

	sm->start_time = timer_query();

	event_process_all();

	return sm->done ? lvlinfo[sm->ship_idx].level : -1;
}

}

#include "webm_player.hpp"

#include "args.h"
#include "inferno.h"
#include "digi.h"
#include "songs.h"
#include "physfsx.h"

namespace d1x
{

static bool GameCfg_MovieTexFilt = true;

static void video_callback(const uint8_t *rgb, int w, int h, void *data)
{
	float scale = 1.0;
	int dstx, dsty;
	grs_bitmap *bm = static_cast<grs_bitmap *>(data);

	if ((static_cast<float>(SWIDTH)/SHEIGHT) < (static_cast<float>(w)/h))
		scale = (static_cast<float>(SWIDTH)/w);
	else
		scale = (static_cast<float>(SHEIGHT)/h);

	dstx = (SWIDTH/2)-((w*scale)/2);
	dsty = (SHEIGHT/2)-((h*scale)/2);

#if DXX_USE_OGL
	glDisable (GL_BLEND);

	//printf("tex %d w %d h %d x %d y %d fmt %x ", bm->gltexture->handle, w, h, dstx, dsty, bm->gltexture->format);
	glBindTexture(GL_TEXTURE_2D, bm->gltexture->handle);
	glEnable(GL_TEXTURE_2D);
	//while (glGetError()) ;
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb);
	//printf(" ret %x\n", glGetError());
	glDisable(GL_TEXTURE_2D);
	
	//ogl_ubitblt_i(w*scale, h*scale, dstx, dsty,
	//	w, h, 0, 0, *bm, grd_curcanv->cv_bitmap, (GameCfg_MovieTexFilt) ? opengl_texture_filter::trilinear : opengl_texture_filter::classic);

	ogl_ubitmapm_cs(*grd_curcanv, dstx, dsty, w*scale, h*scale, *bm, ogl_colors::white);

	glEnable (GL_BLEND);
	//gr_flip();
#else
	gr_bm_ubitbltm(*grd_curcanv, bufw, bufh, dstx, dsty, 0, 0, source_bm);
#endif
}

struct webm_movie final : ::dcx::window
{
	//using ::dcx::window::window;
	virtual window_event_result event_handler(const d_event &) override;

	RWops_ptr file;
	wp::WebmPlayer player;
	grs_main_bitmap bm;
	webm_movie(grs_canvas &src, int x, int y, int w, int h, RWops_ptr f) :
		window(src, x, y, w, h), file(std::move(f)) {}
};

window_event_result webm_movie::event_handler(const d_event &event)
{
	switch (event.type)
	{
		case EVENT_WINDOW_DRAW:
			if (player.finished())
				return window_event_result::close;
			player.updateVideo();
			break;
			
		case EVENT_KEY_COMMAND:
			if (event_key_get(event) == KEY_ESC)
				return window_event_result::close;
			break;

		case EVENT_WINDOW_CLOSE:
			//printf("movie closed\n");
			#if 0
			player.destroy();
			if (!CGameArg.SndNoSound && CGameArg.SndDisableSdlMixer)
				digi_init();
			#endif
			return window_event_result::ignored;	// continue closing
			break;

		default:
			return window_event_result::ignored;
			break;
	}
	return window_event_result::handled;
}


//returns 1 if played
int play_webm_movie(const char *const filename)
{
	auto &&[filehndl, physfserr] = PHYSFSRWOPS_openRead(filename);
	if (!filehndl)
	{
		con_printf(CON_VERBOSE, "Failed to open movie <%s>: %s", filename, PHYSFS_getErrorByCode(physfserr));
		return 0;
	}

	int ok = 0;
	auto movie = window_create<webm_movie>(grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT,
		std::move(filehndl));

	//if (GameArg.SysNoMovies)
	//	return 0;

	// Stop all digital sounds currently playing.
	digi_stop_digi_sounds();

	// Stop all songs
	songs_stop_all();

	// MD2211: if using SDL_Mixer, we never reinit the sound system
	if (CGameArg.SndDisableSdlMixer)
		digi_close();

	if (movie->player.load(movie->file.get())) {
		gr_init_bitmap_alloc(movie->bm, bm_mode::linear, 0, 0, movie->player.width(), movie->player.height(), movie->player.width());
		memset(const_cast<color_palette_index*>(movie->bm.bm_data), 0, movie->bm.bm_h * movie->bm.bm_rowsize);
		ogl_loadbmtexture_f(movie->bm, (GameCfg_MovieTexFilt) ? opengl_texture_filter::trilinear : opengl_texture_filter::classic, 0, 0);
		//video_callback(NULL, 
		movie->player.setVideoCallback(video_callback, static_cast<void *>(static_cast<grs_bitmap *>(&movie->bm)));
		movie->player.playback(wp::WebmPlayer::PlaybackCommand::Play);
		//movie->player.run();
		ok = 1;
	}

	if (!ok)
		window_close(movie);

	return ok;
}

static const char lvl_exit_movie[] = { 'e', 'e', 'e', 0, // secret
	'f', 'f', 'f', 'a', 'a', 'b', 'b',
	'c', 'c', 'c', 'g', 'f', 'g', 'g', 'f', 'g', 'f', 'g', 'f', 'f', 'f', 'g', 'f', 'g', 'd', 'd', 'f' };

int play_exit_movie(int lvl);

int play_exit_movie(int lvl) {
	char name[] = "d1exitx.webm";
	if (lvl < -3 || lvl > 27)
		return 0;
	name[6] = lvl_exit_movie[lvl + 3];
	if (!PHYSFSX_exists(name, 1))
		return 0;
	
	return play_webm_movie(name);
}

}
