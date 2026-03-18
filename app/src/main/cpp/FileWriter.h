//
// Created by djshaji on 3/16/26.
//

#ifndef OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
#define OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H

#include "logging_macros.h"
#include "sndfile.h"
#include "AudioBuffer.h"
#include "lame.h"
#include "opus/opusenc.h"
#include "FLAC/stream_encoder.h"
#include "vorbis/vorbisenc.h"
#include "ogg/ogg.h"

typedef enum {
    FILE_TYPE_WAV,
    FILE_TYPE_MP3,
    FILE_TYPE_OPUS,
    FILE_TYPE_FLAC,
    FILE_TYPE_OGG
} FileType;

class FileWriter {
public:
    int sampleRate;
    static int channels;
    static SNDFILE *sndFile;
    static bool recording ;

    SF_INFO sfInfo;
    float quality = 1.f;

    static lame_global_flags * lameGlobalFlags;
    static void * mp3_buffer ;
    static int fileDescriptor;
    static FLAC__StreamEncoder *flacEncoder;
    static FLAC__int32 *flacBuffer;
    static size_t flacBufferSamples;
    static bool vorbisInitialized;
    static vorbis_info vorbisInfo;
    static vorbis_comment vorbisComment;
    static vorbis_dsp_state vorbisDsp;
    static vorbis_block vorbisBlock;
    static ogg_stream_state oggStream;

    static OggOpusEnc *opusEncoder ;

    FileWriter(int sampleRate, int channels);

    ~FileWriter();

    bool open(int fd, FileType fileType, int quality = 0);

    static int encode(AudioBuffer * buffer);

    void close();

    bool openSndfile(int fd, FileType fileType, int _quality);
    bool openLame(int fd, FileType fileType, int _quality);
    bool openOpus(int fd, FileType fileType, int _quality);
    bool openFlac(int fd, FileType fileType, int _quality);
    bool openVorbis(int fd, FileType fileType, int _quality);
    static size_t mp3bufSize ;//= ((384 * 1.25) + 7200) * 2; // Buffer size for MP3 encoding (stereo)
};

#endif //OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
