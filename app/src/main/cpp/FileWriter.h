//
// Created by djshaji on 3/16/26.
//

#ifndef OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
#define OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H

#include "logging_macros.h"
#include "sndfile.h"

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
    int channels;

    SNDFILE *sndFile = nullptr;
    SF_INFO sfInfo;
    bool recording = false;

    FileWriter(int sampleRate, int channels);

    ~FileWriter();

    bool open(const char *filePath, FileType fileType);

    bool write(const float *data, int numFrames);

    void close();
};

#endif //OPIQO_GUITAR_MULTI_EFFECTS_PROCESSOR_FILEWRITER_H
