#define _USE_MATH_DEFINES
//
// g++ soft.cpp -lopenal -std=c++11
//
#include <iostream>
#include <memory>
#include <cmath>
#include <limits>

#include <al.h>
#include <alc.h>
#include <alext.h>

namespace {
    LPALCLOOPBACKOPENDEVICESOFT alcLoopbackOpenDeviceSOFT;
    LPALCISRENDERFORMATSUPPORTEDSOFT alcIsRenderFormatSupportedSOFT;
    LPALCRENDERSAMPLESSOFT alcRenderSamplesSOFT;

    template <typename Fn>
    inline void load_soft_extension(char const* const name, Fn* const fn) {
        *fn = reinterpret_cast<Fn>(::alcGetProcAddress(NULL, name));
        if (!*fn) {
            // TODO: throw excetpion
            std::cerr << "could not load a extension" << name << '\n';
            return;
        }
    }

    template <typename T, typename D>
    inline std::unique_ptr<T, D> make_unique(T* p, D d) {
        return std::unique_ptr<T, D>(p, d);
    }
}

#define ALC_LOAD_SOFT_EXTENSION(p) load_soft_extension(#p, &p)
#define AL_ERROR_CHECK()                         \
  do {                                           \
    if (ALenum const error = ::alGetError()) {   \
      std::cerr << ::alGetString(error) << '\n'; \
    }                                            \
  } while (0)

int main(int argc, char* argv[]) {

    if (!::alcIsExtensionPresent(NULL, "ALC_SOFT_loopback")) {
        std::cerr << "ALC_SOFT_loopback not supported!\n";
        return 1;
    }

    // load extensions
    ALC_LOAD_SOFT_EXTENSION(alcLoopbackOpenDeviceSOFT);
    ALC_LOAD_SOFT_EXTENSION(alcIsRenderFormatSupportedSOFT);
    ALC_LOAD_SOFT_EXTENSION(alcRenderSamplesSOFT);

    // open device
    auto device =
        make_unique(::alcLoopbackOpenDeviceSOFT(NULL), ::alcCloseDevice);
    if (!device) {
        std::cerr << "could not open a device\n";
        return 1;
    }
    {
        ALCint attrs[16] = {
            ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT,  // ステレオ
            ALC_FORMAT_TYPE_SOFT,     ALC_SHORT_SOFT,   // 16bit
            ALC_FREQUENCY,            44100,            // 44.1kHz
            0,                                          // end of attrs
        };
        if (::alcIsRenderFormatSupportedSOFT(device.get(), attrs[5], attrs[1],
            attrs[3]) == ALC_FALSE) {
            std::cerr << "render format not supported: format-channels=" << attrs[1]
                << ", format-type=" << attrs[3] << ", frequency=" << attrs[5]
                << "hz\n";
        }

        // create device context
        auto context =
            make_unique(::alcCreateContext(device.get(), attrs), alcDestroyContext);
        if (!context) {
            std::cerr << "could not create a context\n";
        }
        ::alcMakeContextCurrent(context.get());

        {
            // create a buffer
            ALuint buffer = 0;
            {
                std::cout << "source:";
                ALshort data[64];
                for (int i = 0; i < 64; ++i) {
                    double const x = (2.0 * M_PI) * (i + 0.5) / 64.0;
                    data[i] = static_cast<ALshort>(std::sin(x) *
                        std::numeric_limits<ALshort>::max());
                    std::cout << ' ' << data[i];
                }
                std::cout << '\n';

                ::alGenBuffers(1, &buffer);
                ::alBufferData(buffer, AL_FORMAT_MONO16, data, sizeof(data), 44100);
                AL_ERROR_CHECK();
            }

            // create a source
            ALuint source = 0;
            {
                ::alGenSources(1, &source);
                ::alSourcei(source, AL_BUFFER, buffer);
                ::alSourcei(source, AL_LOOPING, AL_TRUE);  // ループ再生
                ::alSourcef(source, AL_GAIN, 1.0);         // 音量設定
                ::alSourcef(source, AL_PITCH, 1.0);        // ピッチを指定
                ::alSourceRewind(source);                  // 先頭に巻き戻し
                AL_ERROR_CHECK();
            }
            ::alSourcePlay(source);  // 再生開始

            // データを取得
            {
                std::cout << "rendered:";
                ALshort data[128];
                ::alcRenderSamplesSOFT(device.get(), data, 64);
                for (int i = 0; i < 64; ++i) {
                    std::cout << ' ' << data[2 * i] << ':' << data[2 * i + 1];
                }
                std::cout << '\n';
            }

            // 後始末
            ::alSourceStop(source);
            ::alDeleteSources(1, &source);
            ::alDeleteBuffers(1, &buffer);
        }
    }

    return 0;
}