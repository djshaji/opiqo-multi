//
// Created by djshaji on 3/16/26.
//

#include "FileWriter.h"

FileWriter::FileWriter(int _sampleRate, int _channels) {
    sampleRate = _sampleRate;
    channels = _channels;
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = channels;
    sfInfo.format = 0; // Will be set in open()
}

FileWriter::~FileWriter() {

}

bool FileWriter::open(const char *filePath, FileType fileType) {
    switch (fileType) {
        case FILE_TYPE_WAV:
            sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
            break;
        case FILE_TYPE_MP3:
            sfInfo.format = SF_FORMAT_MPEG;
            break;
        case FILE_TYPE_OPUS:
            sfInfo.format = SF_FORMAT_OPUS;
            break;
        case FILE_TYPE_FLAC:
            sfInfo.format = SF_FORMAT_FLAC;
            break;
        case FILE_TYPE_OGG:
            sfInfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
            break;
        default:
            return false; // Unsupported file type
    }

    sndFile = sf_open(filePath, SFM_WRITE, &sfInfo);
    if (!sndFile) {
        int errnum;
        const char *errstr = sf_strerror(nullptr);
        LOGE("Error opening file '%s': %s", filePath, errstr);
        return false; // Failed to open file
    } else {
        recording = true;
        return true; // Successfully opened file
    }

    return false;
}

void FileWriter::close() {
    if (sndFile) {
        sf_close(sndFile);
        sndFile = nullptr;
        recording = false;
    }
}

bool FileWriter::write(const float *data, int numFrames) {
    if (! recording) {
        return false; // Cannot write if not recording
    }

    if (!sndFile) {
        LOGW("Attempted to write to file, but file is not open for recording.");
        return false; // Cannot write if not recording
    }

    sf_count_t framesWritten = sf_writef_float(sndFile, data, numFrames);
    if (framesWritten != numFrames) {
        int errnum;
        const char *errstr = sf_strerror(sndFile);
        LOGE("Error writing to file: %s", errstr);
        return false; // Failed to write all frames
    }

    return true; // Successfully wrote frames
}

