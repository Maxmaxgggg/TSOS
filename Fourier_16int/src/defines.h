#ifndef DEFINES_H
#define DEFINES_H

#include <cstdint>
#if defined(_MSC_VER)
    #define FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCEINLINE inline __attribute__((always_inline))
#else
    #define FORCEINLINE inline
#endif

#define DEFAULT_BLOCK_SIZE   64
#define DEFAULT_INDEX        2
#define DEFAULT_OVERLAP_SIZE 20
#define WAV_SAMP_RATE_OFFSET 24
#define WAV_HEADER_OFFSET    44
#define WAV_CHANNELS_OFFSET  22

/*      Заголовок wav файла     */
struct  WavHeader{
    char        riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t    chunkSize;
    char        wave[4] = {'W', 'A', 'V', 'E'};

    char        fmt[4] = {'f', 'm', 't', ' '};
    uint32_t    subchunk1Size = 16;
    uint16_t    audioFormat = 1;
    uint16_t    numChannels = 2;
    uint32_t    sampleRate;
    uint32_t    byteRate;
    uint16_t    blockAlign;
    uint16_t    bitsPerSample = 16;

    char        data[4] = {'d', 'a', 't', 'a'};
    uint32_t    subchunk2Size;
}__attribute__((packed));
#endif
