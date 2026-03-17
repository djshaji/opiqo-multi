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

bool FileWriter::open(int fd, FileType fileType, int _quality) {
    switch (_quality) {
        case 0:
            quality = 1.f;
            break;
        case 1:
            quality = 0.75f;
            break;
        case 2:
            quality = .5f;
            break;
        default:
            quality = 1.f; // Default to highest quality if invalid value provided
    }

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

    sndFile = sf_open_fd(fd, SFM_WRITE, &sfInfo, 0);
    if (!sndFile) {
        int errnum;
        const char *errstr = sf_strerror(nullptr);
        LOGE("Error opening file '%d': %s", fd, errstr);
        return false; // Failed to open file
    } else {
        sf_command(sndFile, SFC_SET_VBR_ENCODING_QUALITY, &quality, sizeof(float));
        sf_command(sndFile, SFC_SET_COMPRESSION_LEVEL, &quality, sizeof(float));
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

int FileWriter::channels = 2;
SNDFILE *FileWriter::sndFile = nullptr;
bool FileWriter::recording = false;

int FileWriter::write(AudioBuffer * buffer) {
    if (! recording || !sndFile) {
        return false; // Cannot write if not recording
    }

    const float * data = buffer->data;
    int numFrames = buffer->pos / channels; // Assuming pos is the total number of samples (frames * channels)
    sf_count_t framesWritten = sf_writef_float(sndFile, data, numFrames);
    if (framesWritten != numFrames) {
        int errnum;
        const char *errstr = sf_strerror(sndFile);
        LOGE("Error writing to file: %s", errstr);
        return 0; // Failed to write all frames
    }

    return framesWritten; // Successfully wrote frames
}

