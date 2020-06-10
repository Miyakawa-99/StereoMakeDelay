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

///////////StereoGenerate
#include <assert.h>
#include <inttypes.h>
#include <iostream>
#include <windows.h>
#include <al.h>
#include <alc.h>
#include <alext.h>
#include <efx-presets.h>
#include <alhelpers.h>
#include <sndfile.h>

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
static ALuint LoadEffect(const EFXEAXREVERBPROPERTIES* reverb)
{
	ALuint effect = 0;
	ALenum err;
	alGenEffects(1, &effect);
	float reflectionPan =  (0.0000f, 0.0000f, 0.0000f);
	float reverbPan = (0.0000f, 0.0000f, 0.0000f);



	/*printf("Using Chorus\n");
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_CHORUS);

	alEffecti(effect, AL_CHORUS_WAVEFORM, 1);
	alEffecti(effect, AL_CHORUS_PHASE, 90);
	alEffectf(effect, AL_CHORUS_RATE, 1.1f);
	alEffectf(effect, AL_CHORUS_DEPTH, 0.1f);
	alEffectf(effect, AL_CHORUS_FEEDBACK, 0.25f);
	alEffectf(effect, AL_CHORUS_DELAY, 0.0f);*/

	printf("Using Echo\n");
	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_ECHO);

	alEffectf(effect, AL_ECHO_DELAY, 0.0f);
	alEffectf(effect, AL_ECHO_LRDELAY, 0.000f);
	alEffectf(effect, AL_ECHO_DAMPING, 0.0f);
	alEffectf(effect, AL_ECHO_FEEDBACK, 0.0f);
		
	

	/*printf("Using EAX Reverb\n");

	alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

	alEffectf(effect, AL_EAXREVERB_DENSITY, reverb->flDensity);
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


int main(int argc, char* argv[])
{
	EFXEAXREVERBPROPERTIES reverb = { 0.1000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.1000f, 0.1000f, 0.1000f, 0.0000f, 0.3000f, { 0.0000f, 0.0000f, 0.0000f }, 0.0000f, 0.1000f, { 0.0000f, 0.0000f, 0.0000f }, 0.0750f, 0.0000f, 0.0400f, 0.0000f, 0.892f, 1000.0000f, 20.0000f, 0.0000f, 0x1 };

	ALuint source, buffer, effect, slot;
	ALenum state;

	if (argc < 2)
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
	}

//Define a macro to help load the function pointers.
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

	// Load the sound into a buffer.
	buffer = LoadSound(argv[0]);
	if (!buffer)
	{
		CloseAL();
		return 1;
	}

	/* Load the reverb into an effect. */
	effect = LoadEffect(&reverb);
	if (!effect)
	{
		alDeleteBuffers(1, &buffer);
		CloseAL();
		return 1;
	}

	/* Create the effect slot object. This is what "plays" an effect on sources
	 * that connect to it. */
	 alGenAuxiliaryEffectSlots(1, &slot);

	 /* Tell the effect slot to use the loaded effect object. Note that the this
	  * effectively copies the effect properties. You can modify or delete the
	  * effect object afterward without affecting the effect slot.
	  */
	  alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, (ALint)effect);
	  assert(alGetError() == AL_NO_ERROR && "Failed to set effect slot");

	  /* Create the source to play the sound with. */
	  alGenSources(1, &source);
	  alSourcei(source, AL_BUFFER, (ALint)buffer);



	   assert(alGetError() == AL_NO_ERROR && "Failed to setup sound source");

	   /* Play the sound until it finishes. */
	   alSourcePlay(source);

	   for (int i = 0; i <= 360; i++) {
		   alSource3f(source, AL_POSITION, cos(2 * M_PI * i / 360), 0.0, sin(2 * M_PI * i / 360));
		   /* Connect the source to the effect slot. This tells the source to use the
			* effect slot 'slot', on send #0 with the AL_FILTER_NULL filter object.
			*/
		   alSource3i(source, AL_AUXILIARY_SEND_FILTER, (ALint)slot, 0, AL_FILTER_NULL);
		   Sleep(30);
	   }

	   /*do {
		   //al_nssleep(10000000);
		   alGetSourcei(source, AL_SOURCE_STATE, &state);
		   //alSource3f(source, AL_POSITION, -2.0, 0.0, 0.0);

		   if ('\r' == getch()) break;
	   } while (alGetError() == AL_NO_ERROR && state == AL_PLAYING);*/

	   /* All done. Delete resources, and close down OpenAL. */
	   alDeleteSources(1, &source);
	   alDeleteAuxiliaryEffectSlots(1, &slot);
	   alDeleteEffects(1, &effect);
	   alDeleteBuffers(1, &buffer);

	   CloseAL();


	return 0;

}