#include <assert.h>
#include <math.h>
#include <stdio.h>

#include <inttypes.h>
#include <iostream>
#include <assert.h>
#include <windows.h>

#include "SDL.h"
#include "SDL_audio.h"
#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "al.h"
#include "alc.h"
#include "alext.h"
#include "alhelpers.h"
#include <sndfile.h>

#ifndef SDL_AUDIO_MASK_BITSIZE
#define SDL_AUDIO_MASK_BITSIZE (0xFF)
#endif
#ifndef SDL_AUDIO_BITSIZE
#define SDL_AUDIO_BITSIZE(x) (x & SDL_AUDIO_MASK_BITSIZE)
#endif
#define _USE_MATH_DEFINES

typedef struct {
    ALCdevice* Device;
    ALCcontext* Context;
    ALCsizei FrameSize;
} PlaybackInfo;

static LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT;
static LPALCISRENDERFORMATSUPPORTEDSOFT alcIsRenderFormatSupportedSOFT;
static LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT;


void SDLCALL RenderSDLSamples(void* userdata, Uint8* stream, int len)
{
    PlaybackInfo* playback = (PlaybackInfo*)userdata;
    alcRenderSamplesSOFT(playback->Device, stream, len / playback->FrameSize);
}


static const char* ChannelsName(ALCenum chans)
{
    switch (chans)
    {
    case ALC_MONO_SOFT: return "Mono";
    case ALC_STEREO_SOFT: return "Stereo";
    case ALC_QUAD_SOFT: return "Quadraphonic";
    case ALC_5POINT1_SOFT: return "5.1 Surround";
    case ALC_6POINT1_SOFT: return "6.1 Surround";
    case ALC_7POINT1_SOFT: return "7.1 Surround";
    }
    return "Unknown Channels";
}

static const char* TypeName(ALCenum type)
{
    switch (type)
    {
    case ALC_BYTE_SOFT: return "S8";
    case ALC_UNSIGNED_BYTE_SOFT: return "U8";
    case ALC_SHORT_SOFT: return "S16";
    case ALC_UNSIGNED_SHORT_SOFT: return "U16";
    case ALC_INT_SOFT: return "S32";
    case ALC_UNSIGNED_INT_SOFT: return "U32";
    case ALC_FLOAT_SOFT: return "Float32";
    }
    return "Unknown Type";
}

/* Creates a one second buffer containing audioFile, and returns the new
 * buffer ID. */
static ALuint LoadSound(const char* filename)
{
    ALenum err, format;
    ALuint buffer,source;
    SNDFILE* sndfile;
    SF_INFO sfinfo;
    short* membuf;
    sf_count_t num_frames;
    ALsizei num_bytes;

    /* Open the audio file and check that it's usable. */
    sndfile = sf_open(filename, SFM_READ, &sfinfo);
    if (!sndfile)
    {
        fprintf(stderr, "Could not open audio in %s: %s\n", filename, sf_strerror(sndfile));
        return 0;
    }
    if (sfinfo.frames < 1 || sfinfo.frames >(sf_count_t)(INT_MAX / sizeof(short)) / sfinfo.channels)
    {
        fprintf(stderr, "Bad sample count in %s (%" PRId64 ")\n", filename, sfinfo.frames);
        sf_close(sndfile);
        return 0;
    }

    /* Get the sound format, and figure out the OpenAL format */
    if (sfinfo.channels == 1) {
        format = AL_FORMAT_MONO16;
        fprintf(stderr, "1 channels \n");
    }
    else if (sfinfo.channels == 2) {
        format = AL_FORMAT_MONO16;
        fprintf(stderr, "2 channels \n");
    }
    else
    {
        fprintf(stderr, "Unsupported channel count: %d\n", sfinfo.channels);
        sf_close(sndfile);
        return 0;
    }

    /* Decode the whole audio file to a buffer. */
    membuf = (short*)malloc((size_t)(sfinfo.frames * sfinfo.channels) * sizeof(short));


    //ステレオだった場合モノラルに統合
    /*if (sfinfo.channels == 2) {
        for (int i = 0; i < sfinfo.frames; i += 2) {
            //平均値をとる
            membuf[i / 2] = membuf[i] / 2 + membuf[i + 1] / 2;
        }
        //長さもモノラルに直す
        sfinfo.frames = sfinfo.frames / sfinfo.channels;
    }*/


    num_frames = sf_readf_short(sndfile, membuf, sfinfo.frames);
    if (num_frames < 1)
    {
        free(membuf);
        sf_close(sndfile);
        fprintf(stderr, "Failed to read samples in %s (%" PRId64 ")\n", filename, num_frames);
        return 0;
    }
    num_bytes = (ALsizei)(num_frames * sfinfo.channels) * (ALsizei)sizeof(short);

    /* Buffer the audio data into a new buffer object, then free the data and
     * close the file.
     */
    buffer = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, membuf, num_bytes, sfinfo.samplerate);

    free(membuf);
    sf_close(sndfile);

    /* Check if an error occured, and clean up if so. */
    err = alGetError();
    if (err != AL_NO_ERROR)
    {
        fprintf(stderr, "OpenAL Error: %s\n", alGetString(err));
        if (buffer && alIsBuffer(buffer))
            alDeleteBuffers(1, &buffer);
        return 0;
    }


    source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, (ALint)buffer);
    alSource3f(source, AL_POSITION, cos(60), 0.0, sin(60));
    Sleep(30);

    return source;
}


int main(int argc, char* argv[])
{
    PlaybackInfo playback = { NULL, NULL, 0 };
    SDL_AudioSpec desired, obtained;
    ALuint source, buffer;
    ALCint attrs[16];
    ALenum state;
    (void)argc;
    (void)argv;

    /* Print out error if extension is missing. */
    if (!alcIsExtensionPresent(NULL, "ALC_SOFT_loopback"))
    {
        fprintf(stderr, "Error: ALC_SOFT_loopback not supported!\n");
        return 1;
    }

    /* Define a macro to help load the function pointers. */
#define LOAD_PROC(T, x)  ((x) = (T)alcGetProcAddress(NULL, #x))
    LOAD_PROC(LPALCLOOPBACKOPENDEVICESOFT, alcLoopbackOpenDeviceSOFT);
    LOAD_PROC(LPALCISRENDERFORMATSUPPORTEDSOFT, alcIsRenderFormatSupportedSOFT);
    LOAD_PROC(LPALCRENDERSAMPLESSOFT, alcRenderSamplesSOFT);
#undef LOAD_PROC

    if (SDL_Init(SDL_INIT_AUDIO) == -1)
    {
        fprintf(stderr, "Failed to init SDL audio: %s\n", SDL_GetError());
        return 1;
    }

    /* Set up SDL audio with our requested format and callback. */
    desired.channels = 2;
    desired.format = AUDIO_S16SYS;
    desired.freq = 44100;
    desired.padding = 0;
    desired.samples = 4096;
    desired.callback = RenderSDLSamples;
    desired.userdata = &playback;
    if (SDL_OpenAudio(&desired, &obtained) != 0)
    {
        SDL_Quit();
        fprintf(stderr, "Failed to open SDL audio: %s\n", SDL_GetError());
        return 1;
    }

    //////////////LOG
    for (int i = 0; i < SDL_GetNumAudioDevices(0); ++i) {
        SDL_Log("AudioDevice %d: %s", i, SDL_GetAudioDeviceName(i, 0));
    }

    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("AudioDrivers %d: %s\n", i, SDL_GetAudioDriver(i));
    }
    SDL_Log("CurrentAudioDriver: %s\n", SDL_GetCurrentAudioDriver());
    ///////////////

    /* Set up our OpenAL attributes based on what we got from SDL. */
    attrs[0] = ALC_FORMAT_CHANNELS_SOFT;
    if (obtained.channels == 1)
        attrs[1] = ALC_MONO_SOFT;
    else if (obtained.channels == 2)
        attrs[1] = ALC_STEREO_SOFT;
    else
    {
        fprintf(stderr, "Unhandled SDL channel count: %d\n", obtained.channels);
        goto error;
    }

    attrs[2] = ALC_FORMAT_TYPE_SOFT;
    if (obtained.format == AUDIO_U8)
        attrs[3] = ALC_UNSIGNED_BYTE_SOFT;
    else if (obtained.format == AUDIO_S8)
        attrs[3] = ALC_BYTE_SOFT;
    else if (obtained.format == AUDIO_U16SYS)
        attrs[3] = ALC_UNSIGNED_SHORT_SOFT;
    else if (obtained.format == AUDIO_S16SYS)
        attrs[3] = ALC_SHORT_SOFT;
    else if (obtained.format == AUDIO_F32LSB)//add 0624
        attrs[3] = ALC_FLOAT_SOFT;//add 0624

    else
    {
        fprintf(stderr, "Unhandled SDL format: 0x%04x\n", obtained.format);
        goto error;
    }

    attrs[4] = ALC_FREQUENCY;
    attrs[5] = obtained.freq;

    attrs[6] = 0; /* end of list */

    playback.FrameSize = obtained.channels * SDL_AUDIO_BITSIZE(obtained.format) / 8;

    /* Initialize OpenAL loopback device, using our format attributes. */
    playback.Device = alcLoopbackOpenDeviceSOFT(NULL);
    if (!playback.Device)
    {
        fprintf(stderr, "Failed to open loopback device!\n");
        goto error;
    }
    /* Make sure the format is supported before setting them on the device. */
    if (alcIsRenderFormatSupportedSOFT(playback.Device, attrs[5], attrs[1], attrs[3]) == ALC_FALSE)
    {
        fprintf(stderr, "Render format not supported: %s, %s, %dhz\n",
            ChannelsName(attrs[1]), TypeName(attrs[3]), attrs[5]);
        goto error;
    }
    playback.Context = alcCreateContext(playback.Device, attrs);
    if (!playback.Context || alcMakeContextCurrent(playback.Context) == ALC_FALSE)
    {
        fprintf(stderr, "Failed to set an OpenAL audio context\n");
        goto error;
    }

    /* Start SDL playing. Our callback (thus alcRenderSamplesSOFT) will now
     * start being called regularly to update the AL playback state. */
    SDL_PauseAudio(0);

    /* Load the sound into a buffer. */
    source = LoadSound("asano.wav");
    if (!source)
    {
        SDL_CloseAudio();
        alcDestroyContext(playback.Context);
        alcCloseDevice(playback.Device);
        SDL_Quit();
        return 1;
    }

    /* Create the source to play the sound with. */
    /*source = 0;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, (ALint)buffer);*/
    assert(alGetError() == AL_NO_ERROR && "Failed to setup sound source");

    /* Play the sound until it finishes. */
    alSourcePlay(source);
    do {
        al_nssleep(10000000);
        alGetSourcei(source, AL_SOURCE_STATE, &state);
    } while (alGetError() == AL_NO_ERROR && state == AL_PLAYING);

    /* All done. Delete resources, and close OpenAL. */
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);

    /* Stop SDL playing. */
    SDL_PauseAudio(1);

    /* Close up OpenAL and SDL. */
    SDL_CloseAudio();
    alcDestroyContext(playback.Context);
    alcCloseDevice(playback.Device);
    SDL_Quit();

    return 0;

error:
    SDL_CloseAudio();
    if (playback.Context)
        alcDestroyContext(playback.Context);
    if (playback.Device)
        alcCloseDevice(playback.Device);
    SDL_Quit();

    return 1;
}