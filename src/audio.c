#include "audio.h"
#include <math.h>

static Mix_Music *music = NULL;

static float masterMusic = 1.0f;
static float masterSFX   = 1.0f;
static float masterVoice = 1.0f;

static float listenerX = 0.0f;
static float listenerY = 0.0f;

#define SPATIAL_FULL_RADIUS  1600.0f
#define SPATIAL_MAX_RADIUS   6400.0f

// TODO audio files/resonrces SHOUD be managed here, not in the entities

void Audio_initialize(void)
{
	if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0 ) {
		printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
		exit(-1);
	}
	Mix_AllocateChannels(64);
	/* Reserve channels 0-4 so Mix_PlayChannel(-1) won't stomp dedicated channels
	   (1=rebirth, 2=voice, 4=savepoint charge) */
	Mix_ReserveChannels(5);
}

void Audio_cleanup(void)
{
	Mix_Quit();
}

void Audio_loop_music(const char *path)
{
	if (music) Mix_FreeMusic(music);
	music = Mix_LoadMUS(path);
	if (!music) return;
	Mix_PlayMusic(music, -1);
}

void Audio_play_music(const char *path)
{
	if (music) Mix_FreeMusic(music);
	music = Mix_LoadMUS(path);
	if (!music) return;
	Mix_PlayMusic(music, 0);
}

void Audio_stop_music(void)
{
	if (music) {
		Mix_FreeMusic(music);
		music = NULL;
	}
}

void Audio_load_sample(Mix_Chunk **sample, const char *path)
{
	if (!*sample) {
		*sample = Mix_LoadWAV(path);
		if (!*sample) {
			printf("FATAL ERROR: error loading sound: %s\n", path);
			exit(-1);
		}
	}
}

void Audio_unload_sample(Mix_Chunk **sample)
{
	Mix_FreeChunk(*sample);
	*sample = 0;
}

void Audio_play_sample(Mix_Chunk **sample)
{
	if (Mix_PlayChannel(-1, *sample, 0) == -1)
		printf("WARNING: out of audio channels: %s\n", Mix_GetError());
}

void Audio_set_listener_position(float x, float y)
{
	listenerX = x;
	listenerY = y;
}

static float spatial_volume(float x, float y)
{
	float dx = x - listenerX;
	float dy = y - listenerY;
	float dist = sqrtf(dx * dx + dy * dy);
	if (dist <= SPATIAL_FULL_RADIUS)
		return 1.0f;
	if (dist >= SPATIAL_MAX_RADIUS)
		return 0.0f;
	return 1.0f - (dist - SPATIAL_FULL_RADIUS) / (SPATIAL_MAX_RADIUS - SPATIAL_FULL_RADIUS);
}

void Audio_play_sample_at(Mix_Chunk **sample, Position pos)
{
	float vol = spatial_volume((float)pos.x, (float)pos.y);
	if (vol <= 0.0f)
		return;
	int ch = Mix_PlayChannel(-1, *sample, 0);
	if (ch == -1) {
		printf("WARNING: out of audio channels: %s\n", Mix_GetError());
		return;
	}
	Mix_Volume(ch, (int)(vol * masterSFX * MIX_MAX_VOLUME));
}

int Audio_play_sample_on_channel(Mix_Chunk **sample, int channel)
{
	return Mix_PlayChannel(channel, *sample, 0);
}

int Audio_loop_sample_on_channel(Mix_Chunk **sample, int channel)
{
	return Mix_PlayChannel(channel, *sample, -1);
}

void Audio_fade_out_channel(int channel, int ms)
{
	Mix_FadeOutChannel(channel, ms);
}

void Audio_set_master_music(float vol)
{
	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	masterMusic = vol;
	Mix_VolumeMusic((int)(masterMusic * MIX_MAX_VOLUME));
}

float Audio_get_master_music(void) { return masterMusic; }

void Audio_set_master_sfx(float vol)
{
	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	masterSFX = vol;
	for (int ch = 0; ch < 64; ch++) {
		if (ch == VOICE_CHANNEL) continue;
		Mix_Volume(ch, (int)(masterSFX * MIX_MAX_VOLUME));
	}
}

float Audio_get_master_sfx(void) { return masterSFX; }

void Audio_set_master_voice(float vol)
{
	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	masterVoice = vol;
	Mix_Volume(VOICE_CHANNEL, (int)(masterVoice * MIX_MAX_VOLUME));
}

float Audio_get_master_voice(void) { return masterVoice; }

void Audio_boost_sample(Mix_Chunk *chunk, float gain)
{
	Sint16 *samples = (Sint16 *)chunk->abuf;
	int count = chunk->alen / sizeof(Sint16);
	for (int i = 0; i < count; i++) {
		int val = (int)(samples[i] * gain);
		if (val > 32767) val = 32767;
		if (val < -32768) val = -32768;
		samples[i] = (Sint16)val;
	}
}
