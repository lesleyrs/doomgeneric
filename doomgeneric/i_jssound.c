//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2008 David Flater
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	System interface for sound.
//

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "deh_str.h"
#include "i_sound.h"
#include "i_system.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_misc.h"
#include "memio.h"
#include "w_wad.h"
#include "z_zone.h"

#include "doomtype.h"

#include <js/audio.h>

#define NUM_CHANNELS 16

#define NUM_MIDI_CHANNELS 16

typedef struct {
  void *data;
  size_t length;
} SAMPLE;

static boolean sound_initialized = false;

static boolean use_sfx_prefix;


// We don't support libsamplerate with JS but these have to be here since
// other code requires them
int use_libsamplerate = 0;

// Scale factor used when converting libsamplerate floating point numbers
// to integers. Too high means the sounds can clip; too low means they
// will be too quiet. This is an amount that should avoid clipping most
// of the time: with all the Doom IWAD sound effects, at least. If a PWAD
// is used, clipping might occur.

float libsamplerate_scale = 0.65f;


static MEMFILE* WriteWAV(byte *data,
                     uint32_t length, int samplerate)
{
    MEMFILE *wav;
    unsigned int i;
    unsigned short s;

    wav = mem_fopen_write();

    // Header

    mem_fwrite("RIFF", 1, 4, wav);
    i = LONG(36 + length);
    mem_fwrite(&i, 4, 1, wav);
    mem_fwrite("WAVE", 1, 4, wav);

    // Subchunk 1

    mem_fwrite("fmt ", 1, 4, wav);
    i = LONG(16);
    mem_fwrite(&i, 4, 1, wav);           // Length
    s = SHORT(1);
    mem_fwrite(&s, 2, 1, wav);           // Format (PCM)
    s = SHORT(2);
    mem_fwrite(&s, 2, 1, wav);           // Channels (2=stereo)
    i = LONG(samplerate);
    mem_fwrite(&i, 4, 1, wav);           // Sample rate
    i = LONG(samplerate * 2 * 1);
    mem_fwrite(&i, 4, 1, wav);           // Byte rate (samplerate * mono * 8 bit)
    s = SHORT(2 * 1);
    mem_fwrite(&s, 2, 1, wav);           // Block align (mono * 8 bit)
    s = SHORT(8);
    mem_fwrite(&s, 2, 1, wav);           // Bits per sample (8 bit)

    // Data subchunk

    mem_fwrite("data", 1, 4, wav);
    i = LONG(length);
    mem_fwrite(&i, 4, 1, wav);           // Data length
    mem_fwrite(data, 1, length, wav);    // Data

    return wav;
}

// Load and convert a sound effect
// Returns true if successful

static boolean CacheSFX(sfxinfo_t *sfxinfo)
{
	int lumpnum;
	unsigned int lumplen;
	int samplerate;
	unsigned int length;
	byte *data;
	SAMPLE *sample;

	// need to load the sound

	lumpnum = sfxinfo->lumpnum;
	data = W_CacheLumpNum(lumpnum, PU_STATIC);
	lumplen = W_LumpLength(lumpnum);

	// Check the header, and ensure this is a valid sound

	if (lumplen < 8
	 || data[0] != 0x03 || data[1] != 0x00)
	{
		// Invalid sound

		return false;
	}

	// 16 bit sample rate field, 32 bit length field

	samplerate = (data[3] << 8) | data[2];
	length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

	// If the header specifies that the length of the sound is greater than
	// the length of the lump itself, this is an invalid sound lump

	// We also discard sound lumps that are less than 49 samples long,
	// as this is how DMX behaves - although the actual cut-off length
	// seems to vary slightly depending on the sample rate.  This needs
	// further investigation to better understand the correct
	// behavior.

	if (length > lumplen - 8 || length <= 48)
	{
		return false;
	}

	// The DMX sound library seems to skip the first 16 and last 16
	// bytes of the lump - reason unknown.

	data += 16;
	length -= 32;

	sample = malloc(sizeof(SAMPLE));
	if (sample == NULL) {
		return false;
	}
	MEMFILE* f = WriteWAV(data, length, samplerate);
	mem_get_buf(f, &sample->data, &sample->length);

	sfxinfo->driver_data = sample;

	// don't need the original lump any more

	W_ReleaseLumpNum(lumpnum);

	return true;
}


static void GetSfxLumpName(sfxinfo_t *sfx, char *buf, size_t buf_len)
{
	// Linked sfx lumps? Get the lump number for the sound linked to.

	if (sfx->link != NULL)
	{
		sfx = sfx->link;
	}

	// Doom adds a DS* prefix to sound lumps; Heretic and Hexen don't
	// do this.

	if (use_sfx_prefix)
	{
		M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
	}
	else
	{
		M_StringCopy(buf, DEH_String(sfx->name), buf_len);
	}
}


static void I_JS_PrecacheSounds(sfxinfo_t *sounds, int num_sounds)
{
	char namebuf[9];
	int i;

	printf("I_JS_PrecacheSounds: Precaching all sound effects..");

	for (i=0; i<num_sounds; ++i)
	{
		if ((i % 6) == 0)
		{
			printf(".");
			fflush(stdout);
		}

		GetSfxLumpName(&sounds[i], namebuf, sizeof(namebuf));

		sounds[i].lumpnum = W_CheckNumForName(namebuf);

		if (sounds[i].lumpnum != -1)
		{
			CacheSFX(&sounds[i]);
		}
	}

	printf("\n");
}


//
// Retrieve the raw data lump index
//  for a given SFX name.
//

static int I_JS_GetSfxLumpNum(sfxinfo_t *sfx)
{
	char namebuf[9];

	GetSfxLumpName(sfx, namebuf, sizeof(namebuf));

	return W_GetNumForName(namebuf);
}

static void I_JS_UpdateSoundParams(int handle, int vol, int sep)
{
	if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
	{
		return;
	}

	JS_setAudioVolume((float)vol / UINT8_MAX);
	// voice_set_pan(js_voices[handle], sep); // TODO
}

//
// Starting a sound means adding it
//  to the current list of active sounds
//  in the internal channels.
// As the SFX info struct contains
//  e.g. a pointer to the raw data,
//  it is ignored.
// As our sound handling does not handle
//  priority, it is ignored.
// Pitching (that is, increased speed of playback)
//  is set, but currently not used by mixing.
//

static int I_JS_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
	if (!sound_initialized || channel < 0 || channel >= NUM_CHANNELS)
	{
		return -1;
	}

	// Release a sound effect if there is already one playing
	// on this channel
	// if (channels_playing[channel]) {
		// voice_stop(js_voices[channel]);
	// 	channels_playing[channel] = NULL;
	// }

	// Get the sound data
	if (sfxinfo->driver_data == NULL)
	{
		if (!CacheSFX(sfxinfo))
		{
			return -1;
		}
	}
	assert(sfxinfo->driver_data);

	// play sound
	// voice_set_pan(js_voices[channel], sep); // TODO
	JS_setAudioVolume((float)vol / UINT8_MAX);
	SAMPLE *sample = sfxinfo->driver_data;
	JS_startAudio(sample->data, sample->length);

	return channel;
}


static void I_JS_StopSound(int handle)
{
	if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
	{
		return;
	}

	// voice_stop(js_voices[handle]); // TODO
}


static boolean I_JS_SoundIsPlaying(int handle)
{
	int position;

	if (!sound_initialized || handle < 0 || handle >= NUM_CHANNELS)
	{
		return false;
	}

	// position = voice_get_position(js_voices[handle]);
	// if (position < 0) {
	// 	// finished
		return false;
	// }

	// still playing
	// return true;
}

// 
// Periodically called to update the sound system
//

static void I_JS_UpdateSound(void)
{
}


static void I_JS_ShutdownSound(void)
{
	if (!sound_initialized)
	{
		return;
	}

	sound_initialized = false;
}


static boolean I_JS_InitSound(boolean _use_sfx_prefix)
{
	use_sfx_prefix = _use_sfx_prefix;

	sound_initialized = true;

	return true;
}


static snddevice_t sound_js_devices[] =
{
	SNDDEVICE_NONE,
};


sound_module_t DG_sound_module = 
{
	sound_js_devices,
	arrlen(sound_js_devices),
	I_JS_InitSound,
	I_JS_ShutdownSound,
	I_JS_GetSfxLumpNum,
	I_JS_UpdateSound,
	I_JS_UpdateSoundParams,
	I_JS_StartSound,
	I_JS_StopSound,
	I_JS_SoundIsPlaying,
	I_JS_PrecacheSounds,
};

