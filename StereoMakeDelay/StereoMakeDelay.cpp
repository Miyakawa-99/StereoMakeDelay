// StereoMakeDelay.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。

#define _USE_MATH_DEFINES
#define ERROR_CHECK( ret )											\
	if( FAILED( ret ) ){											\
		std::stringstream ss;										\
		ss << "failed " #ret " " << std::hex << ret << std::endl;	\
		throw std::runtime_error( ss.str().c_str() );				\
	}
#define NUM_BUFFER 2
#define NUM_SOURCE 2

#ifndef CHORUSPROPERTIES_DEFINED
#define CHORUSPROPERTIES_DEFINED
typedef struct {
	int flDensity;
	int flDiffusion;
	float flGain;
	float flGainHF;
	float flGainLF;
	float flDecayTime;
	float flDecayHFRatio;
	float flDecayLFRatio;
} CHORUSPROPERTIES, * LPCHORUSPROPERTIES;
#endif
///////////StereoGenerate
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <al.h>
#include <alc.h>
#include <alext.h>
#include <math.h>
#include <conio.h>
#include <efx.h>//20200505
//#include <efx-presets.h>//20200505
#include <alhelpers.h>//20200505
#include <sndfile.hh>//20200505
#include <sndfile.h>//20200505

//////20200603
#include <stdarg.h>
#include <string.h>

#ifndef ALC_ENUMERATE_ALL_EXT
#define ALC_DEFAULT_ALL_DEVICES_SPECIFIER        0x1012
#define ALC_ALL_DEVICES_SPECIFIER                0x1013
#endif

#ifndef ALC_EXT_EFX
#define ALC_EFX_MAJOR_VERSION                    0x20001
#define ALC_EFX_MINOR_VERSION                    0x20002
#define ALC_MAX_AUXILIARY_SENDS                  0x20003
#endif


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN


//#pragma comment(lib, "OpenAL32.lib")
//fopenの警告を無視
#pragma warning(disable:4996)

/* Effect object functions */
static LPALGENEFFECTS alGenEffects;
static LPALDELETEEFFECTS alDeleteEffects;
static LPALISEFFECT alIsEffect;
static LPALEFFECTI alEffecti;
static LPALEFFECTIV alEffectiv;
static LPALEFFECTF alEffectf;
static LPALEFFECTFV alEffectfv;
static LPALGETEFFECTI alGetEffecti;
static LPALGETEFFECTIV alGetEffectiv;
static LPALGETEFFECTF alGetEffectf;
static LPALGETEFFECTFV alGetEffectfv;


/* Auxiliary Effect Slot object functions */
static LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots;
static LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots;
static LPALISAUXILIARYEFFECTSLOT alIsAuxiliaryEffectSlot;
static LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti;
static LPALAUXILIARYEFFECTSLOTIV alAuxiliaryEffectSlotiv;
static LPALAUXILIARYEFFECTSLOTF alAuxiliaryEffectSlotf;
static LPALAUXILIARYEFFECTSLOTFV alAuxiliaryEffectSlotfv;
static LPALGETAUXILIARYEFFECTSLOTI alGetAuxiliaryEffectSloti;
static LPALGETAUXILIARYEFFECTSLOTIV alGetAuxiliaryEffectSlotiv;
static LPALGETAUXILIARYEFFECTSLOTF alGetAuxiliaryEffectSlotf;
static LPALGETAUXILIARYEFFECTSLOTFV alGetAuxiliaryEffectSlotfv;

//static LPALGETAUXILIARYEFFECTSLOTFV flLRDelay; //20200506

////立体音響の生成///////
class StereoGenerate {

public:
	//ファイルを開くときの変数
	char type[4];
	DWORD size, chunkSize;
	short formatType, channels;
	DWORD sampleRate, avgBytesPerSec;
	short bytesPerSample, bitsPerSample;
	unsigned long dataSize;

	//1サンプルの長さ
	unsigned long memory;
	//どのくらい読み込むか
	unsigned long length;
	signed short* wav_data;

	ALuint buffer;
	ALuint sources[NUM_SOURCE];
	ALfloat ListenerPos[3];


	int SoundSet(const char* SoundName) {
		FILE* fp;
		//unsigned long memory = 0, length = 0;

		fp = fopen(SoundName, "rb");
		if (!fp) {
			printf("ファイルを開けない\n");
			return 1;
		}

		//Check that the WAVE file is OK
		fread(type, sizeof(char), 4, fp);
		if (type[0] != 'R' || type[1] != 'I' || type[2] != 'F' || type[3] != 'F') {
			printf("not 'RIFF'\n");
			return 1;
		}
		fread(&size, sizeof(DWORD), 1, fp);
		fread(type, sizeof(char), 4, fp);
		if (type[0] != 'W' || type[1] != 'A' || type[2] != 'V' || type[3] != 'E') {
			printf("not 'WAVE'\n");
			return 1;
		}
		fread(type, sizeof(char), 4, fp);
		if (type[0] != 'f' || type[1] != 'm' || type[2] != 't' || type[3] != ' ') {
			printf("not 'fmt '\n");
			return 1;
		}
		//ここからchunkSizeバイト分がwavのパラメータ領域
		fread(&chunkSize, sizeof(DWORD), 1, fp);
		//基本的にpcm形式
		fread(&formatType, sizeof(short), 1, fp);
		//wavのチャンネル数
		fread(&channels, sizeof(short), 1, fp);
		//サンプリング周波数
		fread(&sampleRate, sizeof(DWORD), 1, fp);
		//1秒間の平均転送レート(=channels*sampleRate*bitsPerSample / 8)
		fread(&avgBytesPerSec, sizeof(DWORD), 1, fp);
		//各サンプル数のbyte数(例えば、16bit, 2channelsなら4)
		fread(&bytesPerSample, sizeof(short), 1, fp);
		//量子化ビット数　(16, 8)
		fread(&bitsPerSample, sizeof(short), 1, fp);

		//波形データ長
		fread(&dataSize, sizeof(unsigned long), 1, fp);
		//1サンプルの長さ
		memory = bitsPerSample / 8;
		//どのくらい読み込むか
		length = dataSize / memory;
		wav_data = new signed short[length];

		/*for (int i = 0; i < 256; i++) {
			fread(type, sizeof(char), 1, fp);
			if (type[0] == 'd') {
				fread(type, sizeof(char), 1, fp);
				if (type[0] == 'a') {
					fread(type, sizeof(char), 1, fp);
					if (type[0] == 't') {
						fread(type, sizeof(char), 1, fp);
						if (type[0] == 'a') {
							break;
						}
					}
				}
			}
			if (i == 255) {
				printf("データがありません。\n");
				return 1;
			}
		}*/

		//波形データ
		fread(wav_data, memory, length, fp);
		fclose(fp);
		return 0;

	}
	int SoundPlay() {
		////////////////////////////////////////////
		const ALCchar* deviceList = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);
		printf(deviceList);
		//デバイスを開く
		ALCdevice* device = alcOpenDevice(nullptr);
		//コンテキストを生成
		ALCcontext* context = alcCreateContext(device, nullptr);
		//生成したコンテキストを操作対象にする
		alcMakeContextCurrent(context);

		//CreateBufferObject & Create Source Object
		alGenBuffers(1, &buffer); //曲データ１つにつきバッファ１つ．
        // 3.source用意
		alGenSources(NUM_SOURCE, sources); //空間に配置する数の分生成する.

		ALuint frequency = sampleRate;

		//ステレオだった場合モノラルに統合
		if (channels == 2) {
			for (int i = 0; i < length; i += 2) {
				//平均値をとる
				wav_data[i / 2] = wav_data[i] / 2 + wav_data[i + 1] / 2;
			}
		}
		//長さもモノラルに直す
		length = length / channels;
		if (bitsPerSample == 8) {
			printf("8bit\n");
			return 0;
		}

		ALuint format = AL_FORMAT_MONO16;
		// bufferによみこみ
		for (int i = 0; i < NUM_BUFFER; i++) {
			alBufferData(buffer, format, &wav_data[0], length * sizeof(signed short), sampleRate);
		}

		// source と buffer の接続
		for (int i = 0; i < NUM_SOURCE; i++) {
			alSourcei(sources[i], AL_BUFFER, buffer);
		}

		// 4.sourceのプロパティ設定
		for (int i = 0; i < NUM_SOURCE; i++) {
			alSourcei(sources[i], AL_LOOPING, AL_TRUE);   // 繰り返し
			alSourcei(sources[i], AL_PITCH, 1.0f);      //
			alSourcei(sources[i], AL_GAIN, 1.0f);     // 音量
			//alSource3f(sources[i], AL_POSITION, 0, 0, 0); // 音源位置
		}
		/*alSource3f(sources[0], AL_POSITION, 0, 0, 0); // 音源位置
		alSource3f(sources[1], AL_POSITION, -1, 0, 0); // 音源位置

		alSourcei(sources[0], AL_SEC_OFFSET, 0.0);
		alSourcei(sources[1], AL_SEC_OFFSET, 0.0);*/

		/*float offset;

		alGetSourcef(source, AL_SEC_OFFSET, &offset);

		return offset;*/
		// 7.再生
		alSourcePlayv(NUM_SOURCE,sources);

		for (int i = 0; i <= 360; i++) {
			alSource3f(sources[0], AL_POSITION, cos(2 * M_PI * i / 360), 0.0, sin(2 * M_PI * i / 360));
			alSource3f(sources[1], AL_POSITION, cos(2 * M_PI * i / 360), 0.0, sin(2 * M_PI * i / 360));
			Sleep(30);

		}
		
		//バッファに格納したので消してよい
		delete[] wav_data;
		printf("再生中\n");
		printf("buffer = %u\n", buffer);
		//printf("source = %u\n", sources);
		/////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////
		/*alDeleteBuffers(1, &buffer);
		alDeleteSources(1, &source);

		//OpenALの後始末
		//操作対象のコンテキストを解除
		alcMakeContextCurrent(nullptr);
		//コンテキストを破棄
		alcDestroyContext(context);
		//デバイスを閉じる
		alcCloseDevice(device);*/

		return 0;
		//ここちゃんと、gestureとかでメモリ解放するように実装する
	}

	void UpdateListener(ALfloat Listener[3], float x, float y, float z) {
		//リスナー(自分)を空間座標に配置
		Listener[0] = x;
		Listener[1] = y;
		Listener[2] = z;
	}
};


/* LoadEffect loads the given reverb properties into a new OpenAL effect
 * object, and returns the new effect ID. */
static ALuint LoadEffect(const CHORUSPROPERTIES* reverb)
{
	ALuint effect = 0;
	ALenum err;
	/* Create the effect object and check if we can do EAX reverb. */
	alGenEffects(1, &effect);
	/* alGenEffects function is used to create one or more Effect objects.An Effect object stores
		an effect type and a set of parameter values to control that Effect.In order to use an Effect it
		must be attached to an Auxiliary Effect Slot object.*/
	//Returns the actual ALenum described by a string.Returns NULL if the string doesn’t
		//describe a valid OpenAL enum.

	if (alGetEnumValue("AL_EFFECT_REVERB") != NULL)
	{
		printf("Using EAX Reverb\n");
		/* EAX Reverb is available. Set the EAX effect type then load the
		 reverb properties. */

		alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);

		/*alEffectf(effect, AL_EAXREVERB_DENSITY, reverb->flDensity);
		alEffectf(effect, AL_EAXREVERB_DIFFUSION, reverb->flDiffusion);
		alEffectf(effect, AL_EAXREVERB_GAIN, reverb->flGain);
		alEffectf(effect, AL_EAXREVERB_GAINHF, reverb->flGainHF);
		alEffectf(effect, AL_EAXREVERB_GAINLF, reverb->flGainLF);
		alEffectf(effect, AL_EAXREVERB_DECAY_TIME, reverb->flDecayTime);
		alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, reverb->flDecayHFRatio);
		alEffectf(effect, AL_EAXREVERB_DECAY_LFRATIO, reverb->flDecayLFRatio);
		alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
		alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
		alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, reverb->flReflectionsPan);
		alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain);
		alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
		alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, reverb->flLateReverbPan);
		alEffectf(effect, AL_EAXREVERB_ECHO_TIME, reverb->flEchoTime);
		alEffectf(effect, AL_EAXREVERB_ECHO_DEPTH, reverb->flEchoDepth);
		alEffectf(effect, AL_EAXREVERB_MODULATION_TIME, reverb->flModulationTime);
		alEffectf(effect, AL_EAXREVERB_MODULATION_DEPTH, reverb->flModulationDepth);
		alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
		alEffectf(effect, AL_EAXREVERB_HFREFERENCE, reverb->flHFReference);
		alEffectf(effect, AL_EAXREVERB_LFREFERENCE, reverb->flLFReference);
		alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
		alEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);*/
		//alEffecti(effect, AL_CHORUS_WAVEFORM, reverb->flLRDelay);/////20200506

	}
	else
	{
		printf("Using Standard Reverb\n");
		/* No EAX Reverb. Set the standard reverb effect type then load the
		 * available reverb properties. */
		alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);

		/*alEffecti(effect, AL_CHORUS_WAVEFORM, reverb->flDensity);
		alEffecti(effect, AL_CHORUS_PHASE, reverb->flDiffusion);
		alEffectf(effect, AL_CHORUS_RATE, reverb->flGain);
		alEffectf(effect, AL_CHORUS_DEPTH, reverb->flGainHF);
		alEffectf(effect, AL_CHORUS_FEEDBACK, reverb->flDecayTime);
		alEffectf(effect, AL_CHORUS_DELAY, reverb->flDecayHFRatio);*/

		/*alEffectf(effect, AL_REVERB_REFLECTIONS_GAIN, reverb->flReflectionsGain);
		alEffectf(effect, AL_REVERB_REFLECTIONS_DELAY, reverb->flReflectionsDelay);
		alEffectf(effect, AL_REVERB_LATE_REVERB_GAIN, reverb->flLateReverbGain);
		alEffectf(effect, AL_REVERB_LATE_REVERB_DELAY, reverb->flLateReverbDelay);
		alEffectf(effect, AL_REVERB_AIR_ABSORPTION_GAINHF, reverb->flAirAbsorptionGainHF);
		alEffectf(effect, AL_REVERB_ROOM_ROLLOFF_FACTOR, reverb->flRoomRolloffFactor);
		alEffecti(effect, AL_REVERB_DECAY_HFLIMIT, reverb->iDecayHFLimit);*/
		//alEffecti(effect, AL_CHORUS_PHASE, reverb->flLRDelay);/////20200506
	}
	
	/* Check if an error occured, and clean up if so. */
	err = alGetError();
	if (err != AL_NO_ERROR)
	{
		fprintf(stderr, "OpenAL LoadEffectError: %s\n", alGetString(err));

		if (alIsEffect(effect))
			alDeleteEffects(1, &effect);
		return 0;
	}

	return effect;
}

/* LoadBuffer loads the named audio file into an OpenAL buffer object, and
 * returns the new buffer ID.
 */
static ALuint LoadSound(const char* filename)
{
	ALenum err, format;
	ALuint buffer;
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
	if (sfinfo.channels == 1)
		format = AL_FORMAT_MONO16;
	else if (sfinfo.channels == 2)
		format = AL_FORMAT_STEREO16;
	else
	{
		fprintf(stderr, "Unsupported channel count: %d\n", sfinfo.channels);
		sf_close(sndfile);
		return 0;
	}

	/* Decode the whole audio file to a buffer. */
	membuf = (short*)malloc((size_t)(sfinfo.frames * sfinfo.channels) * sizeof(short));

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

	return buffer;
}



static WCHAR* FromUTF8(const char* str)
{
	WCHAR* out = NULL;
	int len;

	if ((len = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0)) > 0)
	{
		//membuf = (short*)malloc((size_t)(sfinfo.frames * sfinfo.channels) * sizeof(short));

		out = (WCHAR*)calloc(sizeof(WCHAR), (unsigned int)(len));
		MultiByteToWideChar(CP_UTF8, 0, str, -1, out, len);
	}
	return out;
}

/* Override printf, fprintf, and fwrite so we can print UTF-8 strings. */
static void al_fprintf(FILE* file, const char* fmt, ...)
{
	char str[1024];
	WCHAR* wstr;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);

	str[sizeof(str) - 1] = 0;
	wstr = FromUTF8(str);
	if (!wstr)
		fprintf(file, "<UTF-8 error> %s", str);
	else
		fprintf(file, "%ls", wstr);
	free(wstr);
}
#define fprintf al_fprintf
#define printf(...) al_fprintf(stdout, __VA_ARGS__)

static size_t al_fwrite(const void* ptr, size_t size, size_t nmemb, FILE* file)
{
	char str[1024];
	WCHAR* wstr;
	size_t len;

	len = size * nmemb;
	if (len > sizeof(str) - 1)
		len = sizeof(str) - 1;
	memcpy(str, ptr, len);
	str[len] = 0;

	wstr = FromUTF8(str);
	if (!wstr)
		fprintf(file, "<UTF-8 error> %s", str);
	else
		fprintf(file, "%ls", wstr);
	free(wstr);

	return len / size;
}
#define fwrite al_fwrite
#endif


#define MAX_WIDTH  80

static void printList(const char* list, char separator)
{
	size_t col = MAX_WIDTH, len;
	const char* indent = "    ";
	const char* next;

	if (!list || *list == '\0')
	{
		fprintf(stdout, "\n%s!!! none !!!\n", indent);
		return;
	}

	do {
		next = strchr(list, separator);
		if (next)
		{
			len = (size_t)(next - list);
			do {
				next++;
			} while (*next == separator);
		}
		else
			len = strlen(list);

		if (len + col + 2 >= MAX_WIDTH)
		{
			fprintf(stdout, "\n%s", indent);
			col = strlen(indent);
		}
		else
		{
			fputc(' ', stdout);
			col++;
		}

		len = fwrite(list, 1, len, stdout);
		col += len;

		if (!next || *next == '\0')
			break;
		fputc(',', stdout);
		col++;

		list = next;
	} while (1);
	fputc('\n', stdout);
}

static void printDeviceList(const char* list)
{
	if (!list || *list == '\0')
		printf("    !!! none !!!\n");
	else do {
		printf("    %s\n", list);
		list += strlen(list) + 1;
	} while (*list != '\0');
}


static ALenum checkALErrors(int linenum)
{
	ALenum err = alGetError();
	if (err != AL_NO_ERROR)
		printf("OpenAL Error: %s (0x%x), @ %d\n", alGetString(err), err, linenum);
	return err;
}
#define checkALErrors() checkALErrors(__LINE__)

static ALCenum checkALCErrors(ALCdevice* device, int linenum)
{
	ALCenum err = alcGetError(device);
	if (err != ALC_NO_ERROR)
		printf("ALC Error: %s (0x%x), @ %d\n", alcGetString(device, err), err, linenum);
	return err;
}
#define checkALCErrors(x) checkALCErrors((x),__LINE__)


static void printALCInfo(ALCdevice* device)
{
	ALCint major, minor;

	if (device)
	{
		const ALCchar* devname = NULL;
		printf("\n");
		if (alcIsExtensionPresent(device, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
			devname = alcGetString(device, ALC_ALL_DEVICES_SPECIFIER);
		if (checkALCErrors(device) != ALC_NO_ERROR || !devname)
			devname = alcGetString(device, ALC_DEVICE_SPECIFIER);
		printf("** Info for device \"%s\" **\n", devname);
	}
	alcGetIntegerv(device, ALC_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(device, ALC_MINOR_VERSION, 1, &minor);
	if (checkALCErrors(device) == ALC_NO_ERROR)
		printf("ALC version: %d.%d\n", major, minor);
	if (device)
	{
		printf("ALC extensions:");
		printList(alcGetString(device, ALC_EXTENSIONS), ' ');
		checkALCErrors(device);
	}
}

static void printHRTFInfo(ALCdevice* device)
{
	LPALCGETSTRINGISOFT alcGetStringiSOFT;
	ALCint num_hrtfs;

	if (alcIsExtensionPresent(device, "ALC_SOFT_HRTF") == ALC_FALSE)
	{
		printf("HRTF extension not available\n");
		return;
	}

	alcGetStringiSOFT = (LPALCGETSTRINGISOFT)alcGetProcAddress(device, "alcGetStringiSOFT");

	alcGetIntegerv(device, ALC_NUM_HRTF_SPECIFIERS_SOFT, 1, &num_hrtfs);
	if (!num_hrtfs)
		printf("No HRTFs found\n");
	else
	{
		ALCint i;
		printf("Available HRTFs:\n");
		for (i = 0; i < num_hrtfs; ++i)
		{
			const ALCchar* name = alcGetStringiSOFT(device, ALC_HRTF_SPECIFIER_SOFT, i);
			printf("    %s\n", name);
		}
	}
	checkALCErrors(device);
}

static void printALInfo(void)
{
	printf("OpenAL vendor string: %s\n", alGetString(AL_VENDOR));
	printf("OpenAL renderer string: %s\n", alGetString(AL_RENDERER));
	printf("OpenAL version string: %s\n", alGetString(AL_VERSION));
	printf("OpenAL extensions:");
	printList(alGetString(AL_EXTENSIONS), ' ');
	checkALErrors();
}

static void printResamplerInfo(void)
{
	LPALGETSTRINGISOFT alGetStringiSOFT;
	ALint num_resamplers;
	ALint def_resampler;

	if (!alIsExtensionPresent("AL_SOFT_source_resampler"))
	{
		printf("Resampler info not available\n");
		return;
	}

	alGetStringiSOFT = (LPALGETSTRINGISOFT)alGetProcAddress("alGetStringiSOFT");

	num_resamplers = alGetInteger(AL_NUM_RESAMPLERS_SOFT);
	def_resampler = alGetInteger(AL_DEFAULT_RESAMPLER_SOFT);

	if (!num_resamplers)
		printf("!!! No resamplers found !!!\n");
	else
	{
		ALint i;
		printf("Available resamplers:\n");
		for (i = 0; i < num_resamplers; ++i)
		{
			const ALchar* name = alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i);
			printf("    %s%s\n", name, (i == def_resampler) ? " *" : "");
		}
	}
	checkALErrors();
}

static void printEFXInfo(ALCdevice* device)
{
	ALCint major, minor, sends;
	static const ALchar filters[][32] = {
		"AL_FILTER_LOWPASS", "AL_FILTER_HIGHPASS", "AL_FILTER_BANDPASS", ""
	};
	char filterNames[] = "Low-pass,High-pass,Band-pass,";
	static const ALchar effects[][32] = {
		"AL_EFFECT_EAXREVERB", "AL_EFFECT_REVERB", "AL_EFFECT_CHORUS",
		"AL_EFFECT_DISTORTION", "AL_EFFECT_ECHO", "AL_EFFECT_FLANGER",
		"AL_EFFECT_FREQUENCY_SHIFTER", "AL_EFFECT_VOCAL_MORPHER",
		"AL_EFFECT_PITCH_SHIFTER", "AL_EFFECT_RING_MODULATOR",
		"AL_EFFECT_AUTOWAH", "AL_EFFECT_COMPRESSOR", "AL_EFFECT_EQUALIZER", ""
	};
	static const ALchar dedeffects[][64] = {
		"AL_EFFECT_DEDICATED_DIALOGUE",
		"AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT", ""
	};
	char effectNames[] = "EAX Reverb,Reverb,Chorus,Distortion,Echo,Flanger,"
		"Frequency Shifter,Vocal Morpher,Pitch Shifter,"
		"Ring Modulator,Autowah,Compressor,Equalizer,"
		"Dedicated Dialog,Dedicated LFE,";
	char* current;
	int i;

	if (alcIsExtensionPresent(device, "ALC_EXT_EFX") == AL_FALSE)
	{
		printf("EFX not available\n");
		return;
	}

	alcGetIntegerv(device, ALC_EFX_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(device, ALC_EFX_MINOR_VERSION, 1, &minor);
	if (checkALCErrors(device) == ALC_NO_ERROR)
		printf("EFX version: %d.%d\n", major, minor);
	alcGetIntegerv(device, ALC_MAX_AUXILIARY_SENDS, 1, &sends);
	if (checkALCErrors(device) == ALC_NO_ERROR)
		printf("Max auxiliary sends: %d\n", sends);

	current = filterNames;
	for (i = 0; filters[i][0]; i++)
	{
		char* next = strchr(current, ',');
		ALenum val;

		val = alGetEnumValue(filters[i]);
		if (alGetError() != AL_NO_ERROR || val == 0 || val == -1)
			memmove(current, next + 1, strlen(next));
		else
			current = next + 1;
	}
	printf("Supported filters:");
	printList(filterNames, ',');

	current = effectNames;
	for (i = 0; effects[i][0]; i++)
	{
		char* next = strchr(current, ',');
		ALenum val;

		val = alGetEnumValue(effects[i]);
		if (alGetError() != AL_NO_ERROR || val == 0 || val == -1)
			memmove(current, next + 1, strlen(next));
		else
			current = next + 1;
	}
	if (alcIsExtensionPresent(device, "ALC_EXT_DEDICATED"))
	{
		for (i = 0; dedeffects[i][0]; i++)
		{
			char* next = strchr(current, ',');
			ALenum val;

			val = alGetEnumValue(dedeffects[i]);
			if (alGetError() != AL_NO_ERROR || val == 0 || val == -1)
				memmove(current, next + 1, strlen(next));
			else
				current = next + 1;
		}
	}
	else
	{
		for (i = 0; dedeffects[i][0]; i++)
		{
			char* next = strchr(current, ',');
			memmove(current, next + 1, strlen(next));
		}
	}
	printf("Supported effects:");
	printList(effectNames, ',');
}

int main(int argc, char* argv[])
{
	//EFXEAXREVERBPROPERTIES reverb = EFX_REVERB_PRESET_GENERIC;
	CHORUSPROPERTIES reverb = { 1, 90, 0.3162f, 0.8913f, 1.0000f };

	ALuint source, buffer, effect, slot;
	ALenum state;

	///////////////////
	// create a default device
	//const ALCchar* deviceList = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);


	/*ALCdevice* device = alcOpenDevice(NULL);
	if (!device)
	{
		printf("Could not create OpenAL device.\n");
		return false;
	}

	// context attributes, 2 zeros to terminate 
	ALint attribs[6] = {
		0, 0
	};

	ALCcontext* context = alcCreateContext(device, attribs);
	if (!context)
	{
		printf("Could not create OpenAL context.\n");
		alcCloseDevice(device);
		return false;
	}

	if (!alcMakeContextCurrent(context))
	{
		printf("Could not enable OpenAL context.\n");
		alcDestroyContext(context);
		alcCloseDevice(device);
		return false;
	}*/


	/*if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [-device <name] <filename>\n", argv[0]);
		return 1;
	}

	argv++; argc--;
	if (InitAL(&argv, &argc) != 0)
		return 1;

	if (!alcIsExtensionPresent(alcGetContextsDevice(alcGetCurrentContext()), "ALC_EXT_EFX"))
	{
		fprintf(stderr, "Error: EFX not supported\n");
		CloseAL();
		return 1;
	}*/

	/* Define a macro to help load the function pointers.
#define LOAD_PROC(T, x)  ((x) = (T)alGetProcAddress(#x))
	LOAD_PROC(LPALGENEFFECTS, alGenEffects);
	LOAD_PROC(LPALDELETEEFFECTS, alDeleteEffects);
	LOAD_PROC(LPALISEFFECT, alIsEffect);
	LOAD_PROC(LPALEFFECTI, alEffecti);
	LOAD_PROC(LPALEFFECTIV, alEffectiv);
	LOAD_PROC(LPALEFFECTF, alEffectf);
	LOAD_PROC(LPALEFFECTFV, alEffectfv);
	LOAD_PROC(LPALGETEFFECTI, alGetEffecti);
	LOAD_PROC(LPALGETEFFECTIV, alGetEffectiv);
	LOAD_PROC(LPALGETEFFECTF, alGetEffectf);
	LOAD_PROC(LPALGETEFFECTFV, alGetEffectfv);

	LOAD_PROC(LPALGENAUXILIARYEFFECTSLOTS, alGenAuxiliaryEffectSlots);
	LOAD_PROC(LPALDELETEAUXILIARYEFFECTSLOTS, alDeleteAuxiliaryEffectSlots);
	LOAD_PROC(LPALISAUXILIARYEFFECTSLOT, alIsAuxiliaryEffectSlot);
	LOAD_PROC(LPALAUXILIARYEFFECTSLOTI, alAuxiliaryEffectSloti);
	LOAD_PROC(LPALAUXILIARYEFFECTSLOTIV, alAuxiliaryEffectSlotiv);
	LOAD_PROC(LPALAUXILIARYEFFECTSLOTF, alAuxiliaryEffectSlotf);
	LOAD_PROC(LPALAUXILIARYEFFECTSLOTFV, alAuxiliaryEffectSlotfv);
	LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTI, alGetAuxiliaryEffectSloti);
	LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTIV, alGetAuxiliaryEffectSlotiv);
	LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTF, alGetAuxiliaryEffectSlotf);
	LOAD_PROC(LPALGETAUXILIARYEFFECTSLOTFV, alGetAuxiliaryEffectSlotfv);
#undef LOAD_PROC

	/* Load the sound into a buffer.
	buffer = LoadSound(argv[0]);
	if (!buffer)
	{
		CloseAL();
		return 1;
	}

	/* Load the reverb into an effect. */
	/*effect = LoadEffect(&reverb);
	if (!effect)
	{
		alDeleteBuffers(1, &buffer);
		CloseAL();
		return 1;
	}

	/* Create the effect slot object. This is what "plays" an effect on sources
	 * that connect to it. */
	 /*slot = 0;
	 alGenAuxiliaryEffectSlots(1, &slot);

	 /* Tell the effect slot to use the loaded effect object. Note that the this
	  * effectively copies the effect properties. You can modify or delete the
	  * effect object afterward without affecting the effect slot.
	  */
	  /*alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, (ALint)effect);
	  assert(alGetError() == AL_NO_ERROR && "Failed to set effect slot");

	  /* Create the source to play the sound with. */
	  /*source = 0;
	  alGenSources(1, &source);
	  alSourcei(source, AL_BUFFER, (ALint)buffer);

	  /* Connect the source to the effect slot. This tells the source to use the
	   * effect slot 'slot', on send #0 with the AL_FILTER_NULL filter object.
	   */
	   /*alSource3i(source, AL_AUXILIARY_SEND_FILTER, (ALint)slot, 0, AL_FILTER_NULL);
	   assert(alGetError() == AL_NO_ERROR && "Failed to setup sound source");

	   /* Play the sound until it finishes. */
	   /*alSourcePlay(source);
	   do {
		   al_nssleep(10000000);
		   alGetSourcei(source, AL_SOURCE_STATE, &state);
		   if ('\r' == getch()) break;
	   } while (alGetError() == AL_NO_ERROR && state == AL_PLAYING);

	   /* All done. Delete resources, and close down OpenAL. */
	   /*alDeleteSources(1, &source);
	   alDeleteAuxiliaryEffectSlots(1, &slot);
	   alDeleteEffects(1, &effect);
	   alDeleteBuffers(1, &buffer);

	   CloseAL();

	   return 0;*/


	ALCdevice* device;
	ALCcontext* context;
	const ALCchar* devicename = 0;


	if (argc > 1 && (strcmp(argv[1], "--help") == 0 ||
		strcmp(argv[1], "-h") == 0))
	{
		printf("Usage: %s [playback device]\n", argv[0]);
		return 0;
	}

	printf("Available playback devices:\n");
	if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
		printDeviceList(alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER));
	else
		printDeviceList(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
	printf("Available capture devices:\n");
	printDeviceList(alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER));

	if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE)
		printf("Default playback device: %s\n",
			alcGetString(NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER));
	else
		printf("Default playback device: %s\n",
			alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
	printf("Default capture device: %s\n",
		alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER));

	printALCInfo(NULL);


	device = alcOpenDevice(devicename);
	//device = alcOpenDevice("Generic Software on");
	if (!device)
	{
		printf("\n!!! Failed to open %s !!!\n\n", ((argc > 1) ? argv[1] : "default device"));
		return 1;
	}
	printALCInfo(device);
	printHRTFInfo(device);

	context = alcCreateContext(device, NULL);
	if (!context || alcMakeContextCurrent(context) == ALC_FALSE)
	{
		if (context)
			alcDestroyContext(context);
		alcCloseDevice(device);
		printf("\n!!! Failed to set a context !!!\n\n");
		return 1;
	}

	printALInfo();
	printResamplerInfo();
	printEFXInfo(device);

	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}