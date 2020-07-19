#include <al.h>
#include <alc.h>
#include <iostream>

#define SAMPLINGRATE 44100 
#define BUFFER_SIZE 4410

using namespace std;
int main(void) {
	//マイクセットアップ
	ALCdevice* mic = alcCaptureOpenDevice(NULL, SAMPLINGRATE, AL_FORMAT_MONO16, BUFFER_SIZE);
	//スピーカーセットアップ
	ALCdevice* speaker = alcOpenDevice(NULL);
	//いつもの
	ALCcontext* context = alcCreateContext(speaker, NULL);
	alcMakeContextCurrent(context);

	//バッファ(保存領域)とソース(音源)を宣言
	//ストリーミング用にバッファを二つ
	ALuint buffer[2];
	ALuint source;
	//それを生成
	alGenBuffers(2, &buffer[0]);
	alGenSources(1, &source);

	//マイクから録音した音を一旦入れておく
	ALshort* store = new ALshort[BUFFER_SIZE];

	//再生状態を監視するための準備
	alBufferData(buffer[0], AL_FORMAT_MONO16, &store[0], 0, SAMPLINGRATE);
	alSourceQueueBuffers(source, 1, &buffer[0]);
	alSourcePlay(source);

	//録音開始
	alcCaptureStart(mic);
	cout << "録音＆再生中・・・" << endl;

	//録音と再生を制御
	int a = 0, count = 100;
	ALint sample, state;
	while (1) {
		//sourceが再生中か確認
		alGetSourcei(source, AL_BUFFERS_PROCESSED, &state);
		//録音可能なバッファ長を取得
		alcGetIntegerv(mic, ALC_CAPTURE_SAMPLES, sizeof(sample), &sample);
		//再生が終わり、録音が可能なとき
		if (sample > 0 && state == 1) {
			//録音してstoreに格納
			alcCaptureSamples(mic, (ALCvoid*)&store[0], sample);
			//再生バッファをソースから外す
			alSourceUnqueueBuffers(source, 1, &buffer[a]);
			//待機バッファをソースに適用
			alSourceQueueBuffers(source, 1, &buffer[a ^ 1]);
			//再生
			alSourcePlay(source);
			//待機バッファに録音した音を入れる
			alBufferData(buffer[a], AL_FORMAT_MONO16, &store[0], sample * sizeof(unsigned short), SAMPLINGRATE);
			//ここでバッファの切り替え
			a = a ^ 1;
			count--;
		}
		if (count <= 0) {
			break;
		}
	}
	//マイク停止
	alcCaptureStop(mic);

	//バッファ・ソースの後始末
	alDeleteBuffers(2, &buffer[0]);
	alDeleteSources(1, &source);

	//OpenALの後始末
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	//マイクを閉じる
	alcCloseDevice(mic);
	//スピーカを閉じる
	alcCloseDevice(speaker);
}