//
// Created by djshaji on 3/16/26.
//

#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "FileWriter.h"

static bool writeAllBytes(int fd, const void *buffer, size_t length) {
    const unsigned char *ptr = static_cast<const unsigned char *>(buffer);
    size_t remaining = length;
    while (remaining > 0) {
        ssize_t written = write(fd, ptr, remaining);
        if (written <= 0) {
            return false;
        }
        ptr += written;
        remaining -= static_cast<size_t>(written);
    }
    return true;
}

static bool writeOggPage(int fd, ogg_page *page) {
    return writeAllBytes(fd, page->header, static_cast<size_t>(page->header_len))
        && writeAllBytes(fd, page->body, static_cast<size_t>(page->body_len));
}

static bool drainVorbisStream(bool forceFlush) {
    ogg_page page;
    int pageResult = forceFlush
        ? ogg_stream_flush(&FileWriter::oggStream, &page)
        : ogg_stream_pageout(&FileWriter::oggStream, &page);

    while (pageResult != 0) {
        if (!writeOggPage(FileWriter::fileDescriptor, &page)) {
            LOGE("Failed to write Ogg page: %s", strerror(errno));
            return false;
        }
        pageResult = forceFlush
            ? ogg_stream_flush(&FileWriter::oggStream, &page)
            : ogg_stream_pageout(&FileWriter::oggStream, &page);
    }

    return true;
}

static FLAC__StreamEncoderWriteStatus flacWriteCallback(const FLAC__StreamEncoder *encoder,
                                                        const FLAC__byte buffer[],
                                                        size_t bytes,
                                                        unsigned samples,
                                                        unsigned current_frame,
                                                        void *client_data) {
    int fd = *reinterpret_cast<int *>(client_data);
    ssize_t written = write(fd, buffer, bytes);
    if (written < 0 || static_cast<size_t>(written) != bytes) {
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus flacSeekCallback(const FLAC__StreamEncoder *encoder,
                                                      FLAC__uint64 absolute_byte_offset,
                                                      void *client_data) {
    int fd = *reinterpret_cast<int *>(client_data);
    off_t result = lseek(fd, static_cast<off_t>(absolute_byte_offset), SEEK_SET);
    return (result < 0) ? FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR : FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus flacTellCallback(const FLAC__StreamEncoder *encoder,
                                                      FLAC__uint64 *absolute_byte_offset,
                                                      void *client_data) {
    int fd = *reinterpret_cast<int *>(client_data);
    off_t result = lseek(fd, 0, SEEK_CUR);
    if (result < 0) {
        return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    }
    *absolute_byte_offset = static_cast<FLAC__uint64>(result);
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

lame_report_function logg = [](const char *format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    LOGD("%s", buffer);
};

FileWriter::FileWriter(int _sampleRate, int _channels) {
    sampleRate = _sampleRate;
    channels = _channels;

}

FileWriter::~FileWriter() {

}

bool FileWriter::openSndfile(int fd, FileType fileType, int _quality) {
    sfInfo = {0};
    sfInfo.samplerate = sampleRate;
    sfInfo.channels = channels;
    sfInfo.format = 0; // Will be set in open()

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
            sfInfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
            break;
        case FILE_TYPE_MP3:
            sfInfo.format = SF_FORMAT_MPEG;
            break;
        case FILE_TYPE_OPUS:
            sfInfo.format = SF_FORMAT_OPUS;
            break;
        case FILE_TYPE_FLAC:
            sfInfo.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
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
        ope_encoder_drain(opusEncoder);
        ope_encoder_destroy(opusEncoder);
        opusEncoder = nullptr;
    }

    if (flacEncoder) {
        FLAC__stream_encoder_finish(flacEncoder);
        FLAC__stream_encoder_delete(flacEncoder);
        flacEncoder = nullptr;
    }

    if (flacBuffer) {
        free(flacBuffer);
        flacBuffer = nullptr;
        flacBufferSamples = 0;
    }

    if (vorbisInitialized) {
        vorbis_analysis_wrote(&vorbisDsp, 0);
        while (vorbis_analysis_blockout(&vorbisDsp, &vorbisBlock) == 1) {
            vorbis_analysis(&vorbisBlock, nullptr);
            vorbis_bitrate_addblock(&vorbisBlock);

            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&vorbisDsp, &packet)) {
                ogg_stream_packetin(&oggStream, &packet);
                if (!drainVorbisStream(true)) {
                    break;
                }
            }
        }

        ogg_stream_clear(&oggStream);
        vorbis_block_clear(&vorbisBlock);
        vorbis_dsp_clear(&vorbisDsp);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        vorbisInitialized = false;
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
OggOpusEnc * FileWriter::opusEncoder = nullptr;
FLAC__StreamEncoder * FileWriter::flacEncoder = nullptr;
FLAC__int32 * FileWriter::flacBuffer = nullptr;
size_t FileWriter::flacBufferSamples = 0;
bool FileWriter::vorbisInitialized = false;
vorbis_info FileWriter::vorbisInfo;
vorbis_comment FileWriter::vorbisComment;
vorbis_dsp_state FileWriter::vorbisDsp;
vorbis_block FileWriter::vorbisBlock;
ogg_stream_state FileWriter::oggStream;

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
        int result = ope_encoder_write_float(opusEncoder, data, numFrames);
        if (result != OPE_OK) {
            LOGE("Error encoding Opus data: %s", ope_strerror(result));
            return 0;
        }
        return numFrames;
    }

    if (flacEncoder) {
        const size_t requiredSamples = static_cast<size_t>(numFrames) * static_cast<size_t>(channels);
        if (requiredSamples == 0) {
            return 0;
        }

        if (requiredSamples > flacBufferSamples) {
            FLAC__int32 *newBuffer = static_cast<FLAC__int32 *>(realloc(flacBuffer, requiredSamples * sizeof(FLAC__int32)));
            if (!newBuffer) {
                LOGE("Unable to allocate FLAC conversion buffer for %zu samples", requiredSamples);
                return 0;
            }
            flacBuffer = newBuffer;
            flacBufferSamples = requiredSamples;
        }

        for (size_t i = 0; i < requiredSamples; ++i) {
            float v = data[i];
            if (v > 1.0f) {
                v = 1.0f;
            } else if (v < -1.0f) {
                v = -1.0f;
            }
            flacBuffer[i] = static_cast<FLAC__int32>(lrintf(v * 32767.0f));
        }

        FLAC__bool ok = FLAC__stream_encoder_process_interleaved(flacEncoder, flacBuffer, static_cast<unsigned>(numFrames));
        if (!ok) {
            LOGE("Error encoding FLAC data: %s", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(flacEncoder)]);
            return 0;
        }

        return numFrames;
    }

    if (vorbisInitialized) {
        if (numFrames <= 0) {
            return 0;
        }

        float **analysisBuffer = vorbis_analysis_buffer(&vorbisDsp, numFrames);
        for (int i = 0; i < numFrames; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                analysisBuffer[ch][i] = data[(i * channels) + ch];
            }
        }
        vorbis_analysis_wrote(&vorbisDsp, numFrames);

        while (vorbis_analysis_blockout(&vorbisDsp, &vorbisBlock) == 1) {
            vorbis_analysis(&vorbisBlock, nullptr);
            vorbis_bitrate_addblock(&vorbisBlock);

            ogg_packet packet;
            while (vorbis_bitrate_flushpacket(&vorbisDsp, &packet)) {
                ogg_stream_packetin(&oggStream, &packet);
                if (!drainVorbisStream(false)) {
                    return 0;
                }
            }
        }

        return numFrames;
    }

    return 0; // No file open to write to
}

bool FileWriter::open(int fd, FileType fileType, int quality) {
    fileDescriptor = fd;
    switch (fileType) {
        case FILE_TYPE_WAV:
            return openSndfile(fd, fileType, quality);
        case FILE_TYPE_OPUS:
            return openOpus(fd, fileType, quality);
        case FILE_TYPE_MP3:
            return openLame(fd, fileType, quality);
        case FILE_TYPE_FLAC:
            return openFlac(fd, fileType, quality);
        case FILE_TYPE_OGG:
            return openVorbis(fd, fileType, quality);
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

static int opusWriteCallback(void *user_data, const unsigned char *ptr, opus_int32 len) {
    int fd = *reinterpret_cast<int *>(user_data);
    ssize_t written = write(fd, ptr, len);
    return (written < 0) ? 1 : 0;
}

static int opusCloseCallback(void *user_data) {
    return 0; // fd lifecycle is managed by the caller
}

bool FileWriter::openOpus(int fd, FileType fileType, int _quality) {
    int error = OPE_OK;
    OggOpusComments *comments = ope_comments_create();
    OpusEncCallbacks callbacks = { opusWriteCallback, opusCloseCallback };
    opusEncoder = ope_encoder_create_callbacks(&callbacks, &fileDescriptor, comments, sampleRate, channels, 0, &error);
    ope_comments_destroy(comments);

    if (error != OPE_OK) {
        LOGE("Failed to create Opus encoder: %s", ope_strerror(error));
        return false;
    }

    switch (_quality) {
        case 0:
        default:
            ope_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(OPUS_BITRATE_MAX));
            break;
        case 1:
            ope_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(64000));
            break;
        case 2:
            ope_encoder_ctl(opusEncoder, OPUS_SET_BITRATE(32000));
            break;
    }

    recording = true;
    LOGD("Initialized Opus encoder with sample rate: %d, channels: %d, quality: %d", sampleRate, channels, _quality);
    return true;
}

bool FileWriter::openFlac(int fd, FileType fileType, int _quality) {
    if (fileType != FILE_TYPE_FLAC) {
        return false;
    }

    flacEncoder = FLAC__stream_encoder_new();
    if (!flacEncoder) {
        LOGE("Failed to create FLAC stream encoder");
        return false;
    }

    FLAC__stream_encoder_set_channels(flacEncoder, channels);
    FLAC__stream_encoder_set_sample_rate(flacEncoder, sampleRate);
    FLAC__stream_encoder_set_bits_per_sample(flacEncoder, 32);

    unsigned compressionLevel = 8;
    switch (_quality) {
        case 0:
        default:
            compressionLevel = 8;
            break;
        case 1:
            compressionLevel = 5;
            break;
        case 2:
            compressionLevel = 2;
            break;
    }
    FLAC__stream_encoder_set_compression_level(flacEncoder, compressionLevel);

    FLAC__StreamEncoderInitStatus status = FLAC__stream_encoder_init_stream(
            flacEncoder,
            flacWriteCallback,
            flacSeekCallback,
            flacTellCallback,
            nullptr,
            &fileDescriptor);

    if (status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        LOGE("Failed to initialize FLAC stream encoder: %s", FLAC__StreamEncoderInitStatusString[status]);
        FLAC__stream_encoder_delete(flacEncoder);
        flacEncoder = nullptr;
        return false;
    }

    recording = true;
    LOGD("Initialized FLAC encoder with sample rate: %d, channels: %d, compression: %u", sampleRate, channels, compressionLevel);
    return true;
}

bool FileWriter::openVorbis(int fd, FileType fileType, int _quality) {
    if (fileType != FILE_TYPE_OGG) {
        return false;
    }

    vorbis_info_init(&vorbisInfo);

    float vorbisQuality;
    switch (_quality) {
        case 0:
        default:
            vorbisQuality = 1.f;
            break;
        case 1:
            vorbisQuality = 0.6f;
            break;
        case 2:
            vorbisQuality = 0.3f;
            break;
    }

    if (vorbis_encode_init_vbr(&vorbisInfo, channels, sampleRate, vorbisQuality) != 0) {
        LOGE("Failed to initialize Vorbis VBR encoder");
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    vorbis_comment_init(&vorbisComment);
    vorbis_comment_add_tag(&vorbisComment, "ENCODER", "opiqo");

    if (vorbis_analysis_init(&vorbisDsp, &vorbisInfo) != 0) {
        LOGE("Failed to initialize Vorbis analysis state");
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    if (vorbis_block_init(&vorbisDsp, &vorbisBlock) != 0) {
        LOGE("Failed to initialize Vorbis block state");
        vorbis_dsp_clear(&vorbisDsp);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    const int serial = (fd << 16) ^ sampleRate ^ channels;
    if (ogg_stream_init(&oggStream, serial) != 0) {
        LOGE("Failed to initialize Ogg stream");
        vorbis_block_clear(&vorbisBlock);
        vorbis_dsp_clear(&vorbisDsp);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    ogg_packet header;
    ogg_packet headerComment;
    ogg_packet headerCode;
    if (vorbis_analysis_headerout(&vorbisDsp, &vorbisComment, &header, &headerComment, &headerCode) != 0) {
        LOGE("Failed to generate Vorbis header packets");
        ogg_stream_clear(&oggStream);
        vorbis_block_clear(&vorbisBlock);
        vorbis_dsp_clear(&vorbisDsp);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    ogg_stream_packetin(&oggStream, &header);
    ogg_stream_packetin(&oggStream, &headerComment);
    ogg_stream_packetin(&oggStream, &headerCode);

    if (!drainVorbisStream(true)) {
        ogg_stream_clear(&oggStream);
        vorbis_block_clear(&vorbisBlock);
        vorbis_dsp_clear(&vorbisDsp);
        vorbis_comment_clear(&vorbisComment);
        vorbis_info_clear(&vorbisInfo);
        return false;
    }

    vorbisInitialized = true;
    recording = true;
    LOGD("Initialized Ogg Vorbis encoder with sample rate: %d, channels: %d, quality: %.2f", sampleRate, channels, vorbisQuality);
    return true;
}
