/* AL/al.h
Copyright (c) 2026 by the Endless Sky 3DS port contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

// The subset of OpenAL 1.1 that the game uses, implemented on top of the 3DS
// DSP in ctr/OpenAL.cpp: a software mixer handles streamed sources with gain,
// pitch and simple positional panning, and feeds one NDSP channel.

typedef char ALboolean;
typedef char ALchar;
typedef int ALint;
typedef unsigned int ALuint;
typedef int ALsizei;
typedef int ALenum;
typedef float ALfloat;
typedef void ALvoid;

#define AL_NONE 0
#define AL_FALSE 0
#define AL_TRUE 1

#define AL_PITCH 0x1003
#define AL_POSITION 0x1004
#define AL_VELOCITY 0x1006
#define AL_LOOPING 0x1007
#define AL_GAIN 0x100A
#define AL_ORIENTATION 0x100F
#define AL_SOURCE_STATE 0x1010
#define AL_INITIAL 0x1011
#define AL_PLAYING 0x1012
#define AL_PAUSED 0x1013
#define AL_STOPPED 0x1014
#define AL_BUFFERS_QUEUED 0x1015
#define AL_BUFFERS_PROCESSED 0x1016
#define AL_REFERENCE_DISTANCE 0x1020
#define AL_ROLLOFF_FACTOR 0x1021
#define AL_MAX_DISTANCE 0x1023

#define AL_FORMAT_MONO8 0x1100
#define AL_FORMAT_MONO16 0x1101
#define AL_FORMAT_STEREO8 0x1102
#define AL_FORMAT_STEREO16 0x1103

#define AL_INVERSE_DISTANCE_CLAMPED 0xD002

void alGenBuffers(ALsizei n, ALuint *buffers);
void alDeleteBuffers(ALsizei n, const ALuint *buffers);
void alBufferData(ALuint buffer, ALenum format, const ALvoid *data, ALsizei size, ALsizei frequency);

void alGenSources(ALsizei n, ALuint *sources);
void alDeleteSources(ALsizei n, const ALuint *sources);
void alSourcef(ALuint source, ALenum param, ALfloat value);
void alSource3f(ALuint source, ALenum param, ALfloat x, ALfloat y, ALfloat z);
void alSourcei(ALuint source, ALenum param, ALint value);
void alGetSourcef(ALuint source, ALenum param, ALfloat *value);
void alGetSourcei(ALuint source, ALenum param, ALint *value);
void alSourcePlay(ALuint source);
void alSourcePause(ALuint source);
void alSourceStop(ALuint source);
void alSourceQueueBuffers(ALuint source, ALsizei n, const ALuint *buffers);
void alSourceUnqueueBuffers(ALuint source, ALsizei n, ALuint *buffers);

void alListenerf(ALenum param, ALfloat value);
void alListenerfv(ALenum param, const ALfloat *values);
void alDistanceModel(ALenum model);
void alDopplerFactor(ALfloat value);
