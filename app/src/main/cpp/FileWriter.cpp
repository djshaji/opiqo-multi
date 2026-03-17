//
// Created by djshaji on 3/16/26.
//

#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include "FileWriter.h"

lame_report_function logg = [](const char *format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    LOGD("%s", buffer);
};

FileWriter::FileWriter(int _sampleRate, int _channels) {
    sampleRate = _sampleRate;
    channels = _channels;
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = channels;
    sfInfo.format = 0; // Will be set in open()

}

FileWriter::~FileWriter() {

}

bool FileWriter::openSndfile(int fd, FileType fileType, int _quality) {
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
    recording = false;
    if (sndFile) {
        sf_close(sndFile);
        sndFile = nullptr;
    }

    if (lameGlobalFlags) {
        unsigned char       *mp3buf  ;
        mp3buf = (unsigned char *) malloc (8192*3);
        lame_encode_flush(lameGlobalFlags, mp3buf, 8192*3);
        write (fileDescriptor, mp3buf, 8192*3);
        free (mp3buf);
        lame_close(lameGlobalFlags) ;
        free(mp3_buffer);

        lameGlobalFlags = nullptr;
    }

    if (opusEncoder) {
        opus_encoder_destroy(opusEncoder);
        opusEncoder = nullptr;
    }

     if (fileDescriptor >= 0) {
        fileDescriptor = -1;
    }
}

int FileWriter::channels = 2;
SNDFILE *FileWriter::sndFile = nullptr;
bool FileWriter::recording = false;
lame_global_struct * FileWriter::lameGlobalFlags = nullptr;
void * FileWriter::mp3_buffer = nullptr;
int FileWriter::fileDescriptor = -1;
size_t FileWriter::mp3bufSize = ((4096 * 1.25) + 7200) * 2; // Buffer size for MP3 encoding (stereo)
OpusEncoder * FileWriter::opusEncoder = nullptr;

int FileWriter::encode(AudioBuffer * buffer) {
    if (! recording) {
        return false; // Cannot write if not recording
    }

    const float * data = buffer->data;
    int numFrames = buffer->pos / channels; // Assuming pos is the total number of samples (frames * channels)

    if (sndFile) {
        sf_count_t framesWritten = sf_writef_float(sndFile, data, numFrames);
        if (framesWritten != numFrames) {
            int errnum;
            const char *errstr = sf_strerror(sndFile);
            LOGE("Error writing to file: %s", errstr);
            return 0; // Failed to write all frames
        }

        return framesWritten; // Successfully wrote frames
    }

    if (lameGlobalFlags) {
        int written = lame_encode_buffer_interleaved_ieee_float(lameGlobalFlags, data, numFrames, (unsigned char *) mp3_buffer, mp3bufSize);
        if (written < 0) {
            LOGF("unable to encode mp3 stream: %d", written);
        } else {
            written = write (fileDescriptor, mp3_buffer, written);
            if (written < 0) {
                LOGF("unable to write mp3 stream: %s", strerror(errno));
            }
        }

//        LOGD("Encoded %d frames into %d bytes of MP3 data", numFrames, written);
        return written; // Placeholder for MP3 encoding and writing logic
    }

    if (opusEncoder) {
        unsigned char opusData[4000]; // Buffer for Opus encoded data
        int opusDataSize = opus_encode_float(opusEncoder, data, numFrames, opusData, sizeof(opusData));
        if (opusDataSize < 0) {
            LOGE("Error encoding Opus data: %s", opus_strerror(opusDataSize));
            return 0; // Failed to encode Opus data
        }

        // Write the encoded Opus data to the file descriptor
        ssize_t bytesWritten = write(fileDescriptor, opusData, opusDataSize);
        if (bytesWritten < 0) {
            LOGE("Error writing Opus data: %s", strerror(errno));
            return 0; // Failed to write Opus data
        }

        return bytesWritten; // Successfully encoded and wrote Opus data
    }

    return 0; // No file open to write to
}

bool FileWriter::open(int fd, FileType fileType, int quality) {
    fileDescriptor = fd;
    switch (fileType) {
        case FILE_TYPE_WAV:
        case FILE_TYPE_OPUS:
        case FILE_TYPE_FLAC:
        case FILE_TYPE_OGG:
            return openSndfile(fd, fileType, quality);
        case FILE_TYPE_MP3:
            return openLame(fd, fileType, quality);
        default:
            return false; // Unsupported file type
    }

    return false;
}

bool FileWriter::openLame(int fd, FileType fileType, int _quality) {
    lameGlobalFlags = lame_init();
    lame_set_errorf(lameGlobalFlags, logg);
    lame_set_debugf(lameGlobalFlags, logg);
    lame_set_msgf(lameGlobalFlags, logg);

    lame_set_num_channels(lameGlobalFlags, channels);
    lame_set_in_samplerate(lameGlobalFlags, sampleRate);
    mp3_buffer =  malloc (mp3bufSize);

    quality = _quality;
    switch (_quality) {
        case 0:
        default:
            lame_set_preset(lameGlobalFlags, INSANE);
            break;
        case 1:
            lame_set_preset(lameGlobalFlags, STANDARD);
            break;
        case 2:
            lame_set_preset(lameGlobalFlags, MEDIUM);
            break;
    }

    lame_init_params(lameGlobalFlags);
    recording = true;
    LOGD("Initialized LAME with sample rate: %d, channels: %d, quality: %f", sampleRate, channels, quality);
    // Do we write the MP3 header here? LAME doesn't have a specific function for writing the MP3 header, but it will generate the necessary headers when encoding the first chunk of PCM data. So we can just return true here and handle the encoding and writing in the write() function.
    return true; // Successfully initialized LAME for MP3 encoding
}

bool FileWriter::openOpus(int fd, FileType fileType, int _quality) {
    int error = 0;
    opusEncoder = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_AUDIO, & error);
    if (error != OPUS_OK) {
        LOGE("Failed to create Opus encoder: %s", opus_strerror(error));
        return false;
    }

    // Set Opus encoder parameters based on quality
    switch (_quality) {
        case 0:
        default:
//            opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(OPUS_AUTO));
            opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            break;
        case 1:
            opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(64000)); // 64 kbps for medium quality
            break;
        case 2:
            opus_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(32000)); // 32 kbps for lower quality
            break;
    }

    recording = true;
    LOGD("Initialized Opus encoder with sample rate: %d, channels: %d, quality: %d", sampleRate, channels, _quality);
    return true; // Successfully initialized Opus encoder for encoding
}

