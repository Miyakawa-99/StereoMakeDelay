#define _USE_MATH_DEFINES
#include < al.h >
#include < alc.h >
#include < array >
#include < cmath > 
#include < limits > 
#include < vector > 


int main() { 
	// OpenAL の 初期化 
	ALCdevice* device = alcOpenDevice(nullptr); 
	ALCcontext* context = alcCreateContext(device, nullptr); 
	alcMakeContextCurrent(context); 
	// バッファ の 生成 
	const int queue_buffer_num = 4; 
	std:: array < ALuint, queue_buffer_num > buffer_id; 
	alGenBuffers( buffer_id.size(), &buffer_id[ 0]); 
	for (size_t i = 0; i < buffer_id.size(); ++ i) { 
		// 一 秒 ぶん の サイン 波 を 生成 
		const size_t pcm_freq = 44100; 
		const float key_freq = 440.0 * (i + 1); 
		std:: array < ALshort, pcm_freq > pcm_data; 
		for (size_t h = 0; h < pcm_data.size(); ++ h) { 
			pcm_data[h] = std:: sin( key_freq * M_PI * 2.0 * h / pcm_freq) * 
				std:: numeric_limits < ALshort >:: max(); 
		} 
		// サイン 波 を バッファ に コピー 
		alBufferData( buffer_id[i], AL_FORMAT_MONO16, &pcm_data[0], 
			pcm_data.size() * sizeof(ALshort), pcm_freq);
	} 
	// ソース の 生成 
	ALuint source_id; alGenSources(1, &source_id); 
	// バッファ を キューイング
	alSourceQueueBuffers(source_id, buffer_id.size(), &buffer_id[ 0]); 
	// ソース の 再生 開始 
	alSourcePlay(source_id); 
	int loop_count = 12; 
	while (loop_count != 0) { 
		// 再生 の 終わっ た バッファ が ある か?? 
		// 再生 の 終わっ た バッファ 数 を processed に 格納 する 
		int processed; 
		alGetSourcei(source_id, AL_BUFFERS_PROCESSED, &processed); 
		if (processed > 0) { 
			// 再生 の 終わっ た バッファ を 再利用 
			std:: vector < ALuint > unqueue_buffer_id(processed); 
			alSourceUnqueueBuffers(source_id, processed, &unqueue_buffer_id[0]); 
			// 再 キューイング
			alSourceQueueBuffers(source_id, processed, &unqueue_buffer_id[0]); 
			--loop_count;
		}
	} 

	// ソース の 破棄 
	alDeleteSources(1, &source_id); 
	// バッファ の 破棄 
	alDeleteBuffers(buffer_id.size(), &buffer_id[0]); 
	// OpenAL の 後始末 
	alcMakeContextCurrent(nullptr); 
	alcDestroyContext(context); 
	alcCloseDevice(device); 
}







