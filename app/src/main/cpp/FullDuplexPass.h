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

#ifndef SAMPLES_FULLDUPLEXPASS_H
#define SAMPLES_FULLDUPLEXPASS_H

#include "LV2Plugin.hpp"
#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>
#include "LockFreeQueue.h"

class FullDuplexPass : public oboe::FullDuplexStream {
public:
    LV2Plugin* plugin = nullptr, *plugin1 = nullptr, *plugin2 = nullptr, *plugin3 = nullptr, *plugin4 = nullptr;
    bool *bypass = nullptr ;
    float * gain = nullptr;
    std::mutex* pluginMutex = nullptr;  // Points to engine's mutex for thread-safe plugin access
    LilvInstance *instance = nullptr;
    LockFreeQueueManager * queueManager = nullptr;

    void setPluginBlockFrames(int32_t frames) {
        mRequestedBlockFrames = frames;
        mFixedBlockSamples = 0; // Force reinit with new block size on next callback.
    }

private:
    int32_t mRequestedBlockFrames = 0;
    int32_t mSamplesPerFrame = 0;
    int32_t mFixedBlockFrames = 0;
    int32_t mFixedBlockSamples = 0;
    int32_t mInputFillSamples = 0;
    std::vector<float> mInputBlock;
    std::vector<float> mOutputBlock;
    std::vector<float> mProcessedQueue;
    int32_t mProcessedReadIndex = 0;

    bool initializeBlockAdapter() {
        if (mFixedBlockSamples > 0) {
            return true;
        }

        if (!getOutputStream()) {
            return false;
        }

        mSamplesPerFrame = std::max(1, getOutputStream()->getChannelCount());
        mFixedBlockFrames = (mRequestedBlockFrames > 0)
                               ? mRequestedBlockFrames
                               : getOutputStream()->getFramesPerBurst();
        if (mFixedBlockFrames <= 0) {
            mFixedBlockFrames = 128; // Conservative fallback if burst size is unavailable.
        }

        mFixedBlockSamples = mFixedBlockFrames * mSamplesPerFrame;
        mInputBlock.assign(static_cast<size_t>(mFixedBlockSamples), 0.0f);
        mOutputBlock.assign(static_cast<size_t>(mFixedBlockSamples), 0.0f);
        mProcessedQueue.clear();
        mProcessedQueue.reserve(static_cast<size_t>(mFixedBlockSamples * 8));
        mInputFillSamples = 0;
        mProcessedReadIndex = 0;
        return true;
    }

    void processPluginChain(float *buffer, int32_t numSamples) {
        if (!(bypass && !*bypass)) {
            return;
        }

        if (pluginMutex) {
            std::lock_guard<std::mutex> lock(*pluginMutex);
            if (plugin1) plugin1->process(buffer, buffer, numSamples);
            if (plugin2) plugin2->process(buffer, buffer, numSamples);
            if (plugin3) plugin3->process(buffer, buffer, numSamples);
            if (plugin4) plugin4->process(buffer, buffer, numSamples);
            return;
        }

        if (plugin1) plugin1->process(buffer, buffer, numSamples);
        if (plugin2) plugin2->process(buffer, buffer, numSamples);
        if (plugin3) plugin3->process(buffer, buffer, numSamples);
        if (plugin4) plugin4->process(buffer, buffer, numSamples);
    }

public:
    virtual oboe::DataCallbackResult
    onBothStreamsReady(
            const void *inputData,
            int   numInputFrames,
            void *outputData,
            int   numOutputFrames) {
        if (!outputData) {
            return oboe::DataCallbackResult::Continue;
        }

        if (!initializeBlockAdapter()) {
            return oboe::DataCallbackResult::Continue;
        }

        // This code assumes the data format for both streams is Float.
        const float *inputFloats = static_cast<const float *>(inputData);
        float *outputFloats = static_cast<float *>(outputData);
        float *outputStart = outputFloats;

        int32_t numInputSamples = inputFloats ? (numInputFrames * mSamplesPerFrame) : 0;
        int32_t numOutputSamples = numOutputFrames * mSamplesPerFrame;

        if (numOutputSamples <= 0) {
            return oboe::DataCallbackResult::Continue;
        }

        int32_t inputReadIndex = 0;
        const float gainValue = gain ? *gain : 1.0f;

        // Stage A: accumulate real input until full blocks are available.
        while (inputReadIndex < numInputSamples) {
            const int32_t samplesNeeded = mFixedBlockSamples - mInputFillSamples;
            const int32_t availableInput = numInputSamples - inputReadIndex;
            const int32_t copyCount = std::min(samplesNeeded, availableInput);

            std::memcpy(mInputBlock.data() + mInputFillSamples,
                        inputFloats + inputReadIndex,
                        static_cast<size_t>(copyCount) * sizeof(float));
            mInputFillSamples += copyCount;
            inputReadIndex += copyCount;

            if (mInputFillSamples == mFixedBlockSamples) {
                std::memcpy(mOutputBlock.data(), mInputBlock.data(),
                            static_cast<size_t>(mFixedBlockSamples) * sizeof(float));
                processPluginChain(mOutputBlock.data(), mFixedBlockSamples);

                for (int32_t i = 0; i < mFixedBlockSamples; ++i) {
                    mOutputBlock[static_cast<size_t>(i)] *= gainValue;
                }

                mProcessedQueue.insert(mProcessedQueue.end(), mOutputBlock.begin(), mOutputBlock.end());
                mInputFillSamples = 0;
            }
        }

        // Stage B: render callback output from processed queue; if insufficient, emit silence.
        const int32_t queuedSamples = static_cast<int32_t>(mProcessedQueue.size()) - mProcessedReadIndex;
        const int32_t samplesFromQueue = std::min(numOutputSamples, std::max(0, queuedSamples));
        if (samplesFromQueue > 0) {
            std::memcpy(outputFloats,
                        mProcessedQueue.data() + mProcessedReadIndex,
                        static_cast<size_t>(samplesFromQueue) * sizeof(float));
            mProcessedReadIndex += samplesFromQueue;
        }

        if (samplesFromQueue < numOutputSamples) {
            std::fill(outputFloats + samplesFromQueue,
                      outputFloats + numOutputSamples,
                      0.0f);
        }

        // Occasionally compact queue storage to keep memory bounded without per-callback erases.
        if (mProcessedReadIndex > 0 && mProcessedReadIndex >= static_cast<int32_t>(mProcessedQueue.size())) {
            mProcessedQueue.clear();
            mProcessedReadIndex = 0;
        } else if (mProcessedReadIndex > (mFixedBlockSamples * 4)) {
            mProcessedQueue.erase(mProcessedQueue.begin(),
                                  mProcessedQueue.begin() + mProcessedReadIndex);
            mProcessedReadIndex = 0;
        }

        if (queueManager && inputFloats) {
            // Pass actual callback-sized output to recorder/analyzer path.
            queueManager->process(const_cast<float *>(inputFloats), outputStart, numOutputSamples);
        }

        return oboe::DataCallbackResult::Continue;
    }
};
#endif //SAMPLES_FULLDUPLEXPASS_H
