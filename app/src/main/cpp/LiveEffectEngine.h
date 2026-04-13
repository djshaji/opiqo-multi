/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OBOE_LIVEEFFECTENGINE_H
#define OBOE_LIVEEFFECTENGINE_H

#include <jni.h>
#include <oboe/Oboe.h>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include "FullDuplexPass.h"
#include "json.hpp"
#include "LockFreeQueue.h"
#include "FileWriter.h"

using json = nlohmann::json;

class LiveEffectEngine : public oboe::AudioStreamDataCallback, public oboe::AudioStreamErrorCallback {
public:
    LiveEffectEngine();
    void initLV2();

    void setRecordingDeviceId(int32_t deviceId);
    void setPlaybackDeviceId(int32_t deviceId);

    /**
     * @param isOn
     * @return true if it succeeds
     */
    bool setEffectOn(bool isOn);

    /*
     * oboe::AudioStreamDataCallback interface implementation
     */
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream *oboeStream,
                                          void *audioData, int32_t numFrames) override;

    /*
     * oboe::AudioStreamErrorCallback interface implementation
     */
    void onErrorBeforeClose(oboe::AudioStream *oboeStream, oboe::Result error) override;
    void onErrorAfterClose(oboe::AudioStream *oboeStream, oboe::Result error) override;

    bool setAudioApi(oboe::AudioApi);
    bool setPluginBlockSize(int32_t frames);
    bool isAAudioRecommended(void);

    LockFreeQueueManager queueManager;
    FileWriter * fileWriter ;

    std::string cacheDir ;
    std::unique_ptr<FullDuplexPass> mDuplexStream;
    std::mutex pluginMutex;  // Protects plugin pointers access
    std::vector<LV2Plugin*> plugins; ///< plugin slots (index 0 == plugin1)
    LilvInstance *instance = nullptr;
    LilvWorld * world = nullptr;

    LilvNode *audio_class_, *control_class_, *atom_class_, *input_class_, * toggle_class_,
        *patch_writable, *rsz_minimumSize_, *rdfs_label, *rdfs_range, *mod_filetypes, *enum_class_ ;

    const LilvPlugins * lv2Plugins = nullptr; // collection of available plugins
    json pluginInfo;
    std::shared_ptr<oboe::AudioStream> mRecordingStream;
    std::shared_ptr<oboe::AudioStream> mPlayStream;
    int32_t sampleRate = oboe::DefaultStreamValues::SampleRate ;
    std::atomic<float> gain{1.0f};
    int pluginCount = 0 ;
    std::atomic<bool> bypass{false};
    // Prefer using the stream's frames-per-burst by default to keep latency low.
    // A value of 0 indicates "use frames-per-burst"; callers can override.
    int blockSize = 0;

private:
    bool              mIsEffectOn = false;
    int32_t           mRecordingDeviceId = oboe::kUnspecified;
    int32_t           mPlaybackDeviceId = oboe::kUnspecified;
    const oboe::AudioFormat mFormat = oboe::AudioFormat::Float; // for easier processing
    oboe::AudioApi    mAudioApi = oboe::AudioApi::AAudio;
    int32_t           mSampleRate = oboe::kUnspecified;
    const int32_t     mInputChannelCount = oboe::ChannelCount::Stereo;
    const int32_t     mOutputChannelCount = oboe::ChannelCount::Stereo;
    oboe::Result openStreams();

    void closeStreams();

    void closeStream(std::shared_ptr<oboe::AudioStream> &stream);

    oboe::AudioStreamBuilder *setupCommonStreamParameters(
        oboe::AudioStreamBuilder *builder);
    oboe::AudioStreamBuilder *setupRecordingStreamParameters(
        oboe::AudioStreamBuilder *builder, int32_t sampleRate);
    oboe::AudioStreamBuilder *setupPlaybackStreamParameters(
        oboe::AudioStreamBuilder *builder);
    void warnIfNotLowLatency(std::shared_ptr<oboe::AudioStream> &stream);
};

#endif  // OBOE_LIVEEFFECTENGINE_H
