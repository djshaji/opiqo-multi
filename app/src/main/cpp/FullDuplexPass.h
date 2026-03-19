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
#include <mutex>
#include "LockFreeQueue.h"

class FullDuplexPass : public oboe::FullDuplexStream {
public:
    LV2Plugin* plugin, *plugin1, *plugin2, *plugin3, *plugin4;
    bool *bypass = nullptr ;
    float * gain = nullptr;
    std::mutex* pluginMutex = nullptr;  // Points to engine's mutex for thread-safe plugin access
    LilvInstance *instance;
    LockFreeQueueManager * queueManager;
    virtual oboe::DataCallbackResult
    onBothStreamsReady(
            const void *inputData,
            int   numInputFrames,
            void *outputData,
            int   numOutputFrames) {
        // Copy the input samples to the output with a little arbitrary gain change.

        if (!outputData) {
            return oboe::DataCallbackResult::Continue;
        }

        // This code assumes the data format for both streams is Float.
        const float *inputFloats = static_cast<const float *>(inputData);
        float *outputFloats = static_cast<float *>(outputData);

        // It also assumes the channel count for each stream is the same.
        int32_t samplesPerFrame = getOutputStream()->getChannelCount();
        int32_t numInputSamples = numInputFrames * samplesPerFrame;
        int32_t numOutputSamples = numOutputFrames * samplesPerFrame;

        // It is possible that there may be fewer input than output samples.
        int32_t samplesToProcess = std::min(numInputSamples, numOutputSamples);
//        for (int32_t i = 0; i < samplesToProcess; i++) {
//            *outputFloats++ = *inputFloats++ * 0.95; // do some arbitrary processing
//             inputFloats += samplesPerFrame;
//             outputFloats += samplesPerFrame;
//        }

        if (samplesToProcess <= 0) {
            // If there are no input samples, just clear the output buffer.
            for (int32_t i = 0; i < numOutputSamples; i++) {
                *outputFloats++ = 0.0; // silence
            }
            return oboe::DataCallbackResult::Continue;
        }

        if (!inputFloats) {
            for (int32_t i = 0; i < samplesToProcess; i++) {
                outputFloats[i] = 0.0f;
            }
            return oboe::DataCallbackResult::Continue;
        }

        memcpy(outputFloats, inputFloats, samplesToProcess * sizeof(float));
        /* Use mutex to protect plugin processing if plugins can be added/removed while the stream is running. */
        if (bypass && !*bypass) {
            if (pluginMutex) {
                std::lock_guard<std::mutex> lock(*pluginMutex);
                if (plugin1)
                    plugin1->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin2)
                    plugin2->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin3)
                    plugin3->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin4)
                    plugin4->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
            } else {
                // Fallback if mutex not initialized (for backward compatibility)
                if (plugin1)
                    plugin1->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin2)
                    plugin2->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin3)
                    plugin3->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
                if (plugin4)
                    plugin4->process(const_cast<float *>(outputFloats), outputFloats, samplesToProcess);
            }
        }

        if (queueManager) {
            queueManager->process(const_cast<float *>(inputFloats), outputFloats, samplesToProcess);
        }

        const float gainValue = gain ? *gain : 1.0f;
        for (int32_t i = 0; i < samplesToProcess; i++) {
            *outputFloats++ *= gainValue; // do some arbitrary processing
        }

//        lilv_instance_connect_port(instance, 0, const_cast<float *>(outputFloats));
//        lilv_instance_connect_port(instance, 1, (void *) inputFloats);
//        lilv_instance_run(instance, samplesToProcess);
        // If there are fewer input samples then clear the rest of the buffer.
        int32_t samplesLeft = numOutputSamples - numInputSamples;
        for (int32_t i = 0; i < samplesLeft; i++) {
            *outputFloats++ = 0.0; // silence
        }

        return oboe::DataCallbackResult::Continue;
    }
};
#endif //SAMPLES_FULLDUPLEXPASS_H
