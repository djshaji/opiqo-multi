//
// Created by djshaji on 3/16/26.
//

#ifndef OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
#define OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H

#include "logging_macros.h"
#include "sndfile.h"
#include "AudioBuffer.h"

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

    FileWriter(int sampleRate, int channels);

    ~FileWriter();

    bool open(int fd, FileType fileType, int quality = 0);

    static int write(AudioBuffer * buffer);

    void close();
};

#endif //OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
