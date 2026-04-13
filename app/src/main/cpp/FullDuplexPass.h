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
#include <atomic>
#include <cstdint>
#include <vector>
#include "SpscRingBuffer.h"
#include "LockFreeQueue.h"

/**
 * @class FullDuplexPass
 * @brief Implements full-duplex audio processing using Oboe.
 *
 * This class handles synchronized audio input and output, providing a mechanism
 * to process incoming audio through a chain of LV2 plugins before sending it to
 * the output stream. It includes a block adapter to convert variable-sized
 * Oboe callbacks into fixed-size blocks required by some audio processing algorithms.
 */
class FullDuplexPass : public oboe::FullDuplexStream {
public:
    /** LV2 plugin instances forming the processing chain. */
    std::vector<LV2Plugin*> plugins; ///< plugin slots (index 0 == plugin1)

    /** Pointer to an atomic boolean flag to bypass all processing. */
    std::atomic<bool>* bypass = nullptr;

    /** Pointer to an atomic float controlling the overall output gain. */
    std::atomic<float>* gain = nullptr;

    /** Mutex for thread-safe access to plugin instances during processing. */
    std::mutex* pluginMutex = nullptr;

    /** Pointer to the Lilv instance for plugin management. */
    LilvInstance *instance = nullptr;

    /** Manager for lock-free queues used for audio data recording or analysis. */
    LockFreeQueueManager * queueManager = nullptr;

    /**
     * @brief Sets the block size for plugin processing.
     *
     * @param frames The number of frames per processing block.
     */
    void setPluginBlockFrames(int32_t frames) {
        mRequestedBlockFrames = frames;
        mFixedBlockSamples = 0; // Force reinit with new block size on next callback.
    }

    /**
     * @brief Return the number of processed blocks that were dropped due to a full ring buffer.
     */
    uint32_t getDroppedProcessedBlocks() const {
        return droppedProcessedBlocks.load(std::memory_order_relaxed);
    }

private:
    int32_t mRequestedBlockFrames = 0; ///< The requested size of processing blocks in frames.
    int32_t mSamplesPerFrame = 0;      ///< Number of samples per audio frame (channels).
    int32_t mFixedBlockFrames = 0;     ///< The actual fixed block size used in frames.
    int32_t mFixedBlockSamples = 0;    ///< Total samples in a fixed processing block.
    int32_t mInputFillSamples = 0;     ///< Number of samples currently stored in the input buffer.

    std::vector<float> mInputBlock;    ///< Buffer for accumulating input samples until a full block is ready.
    std::vector<float> mOutputBlock;   ///< Buffer for storing processed output samples.
    SpscRingBuffer<float> mProcessedQueue; ///< Lock-free SPSC ring buffer for processed samples awaiting output.
    std::atomic<uint32_t> droppedProcessedBlocks{0}; ///< Count of processed blocks dropped when ring buffer is full.

    /**
     * @brief Initializes the block adapter buffers based on the stream configuration.
     *
     * @return true if initialization was successful, false otherwise.
     */
    bool initializeBlockAdapter() {
        if (mFixedBlockSamples > 0) {
            return true;
        }

        if (!getOutputStream()) {
            return false;
        }

        mSamplesPerFrame = std::max(1, getOutputStream()->getChannelCount());
        // Prefer the stream frames-per-burst when no explicit request was set.
        // If a requested block size was set, clamp it so it does not exceed the
        // device's frames-per-burst (avoids creating large blocks that increase latency).
        const int32_t burst = getOutputStream()->getFramesPerBurst();
        if (mRequestedBlockFrames > 0) {
            if (burst > 0) {
                mFixedBlockFrames = std::min(mRequestedBlockFrames, burst);
            } else {
                mFixedBlockFrames = mRequestedBlockFrames;
            }
        } else {
            mFixedBlockFrames = (burst > 0) ? burst : 128;
        }
        if (mFixedBlockFrames <= 0) {
            mFixedBlockFrames = 128; // Conservative fallback if burst size is unavailable.
        }

        mFixedBlockSamples = mFixedBlockFrames * mSamplesPerFrame;
        mInputBlock.assign(static_cast<size_t>(mFixedBlockSamples), 0.0f);
        mOutputBlock.assign(static_cast<size_t>(mFixedBlockSamples), 0.0f);
        // Default capacity: prefer 65536 floats but ensure room for several blocks.
        size_t requestedCapacity = std::max<size_t>(static_cast<size_t>(mFixedBlockSamples * 8), 65536);
        mProcessedQueue.init(requestedCapacity);
        mProcessedQueue.reset();
        mInputFillSamples = 0;
        return true;
    }

    /**
     * @brief Processes an audio buffer through the active plugin chain.
     *
     * @param buffer Pointer to the audio data to be processed (in-place).
     * @param numSamples The number of samples in the buffer.
     */
    void processPluginChain(float *buffer, int32_t numSamples) {
                // Skip processing only when an external bypass flag exists and is true.
                // If `bypass` is nullptr or false, perform processing.
                if (bypass && bypass->load(std::memory_order_acquire)) {
            return;
        }

        if (pluginMutex) {
            // Snapshot plugin pointers while holding the mutex, then release it
            // before calling into plugin process() to avoid holding the mutex
            // for the duration of potentially long operations.
            std::vector<LV2Plugin*> snapshot;
            {
                std::lock_guard<std::mutex> lock(*pluginMutex);
                snapshot = plugins;
            }
            for (LV2Plugin* p : snapshot) {
                if (p && p->enabled) p->process(buffer, buffer, numSamples);
            }
            return;
        }

        for (LV2Plugin* p : plugins) {
            if (p && p->enabled) p->process(buffer, buffer, numSamples);
        }
    }

public:
    /**
     * @brief Implementation of Oboe's onBothStreamsReady callback.
     *
     * This method is called whenever there is data available from the input stream
     * and space available in the output stream. It manages the buffering and
     * block-based processing of the audio data.
     *
     * @param inputData Pointer to incoming audio data.
     * @param numInputFrames Number of frames available in inputData.
     * @param outputData Pointer to memory where processed audio should be written.
     * @param numOutputFrames Number of frames that should be written to outputData.
     * @return DataCallbackResult indicating whether to continue or stop the streams.
     */
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
        const float gainValue = gain ? gain->load(std::memory_order_relaxed) : 1.0f;

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

                // Attempt to push processed block into ring buffer. If the buffer is full,
                // drop the new data (policy: drop newest) and increment diagnostics counter.
                bool pushed = mProcessedQueue.push(mOutputBlock.data(), static_cast<size_t>(mFixedBlockSamples));
                if (!pushed) {
                    droppedProcessedBlocks.fetch_add(1u, std::memory_order_relaxed);
                }
                mInputFillSamples = 0;
            }
        }

        // Stage B: render callback output from processed queue; if insufficient, emit silence.
        const size_t queuedSamples = mProcessedQueue.size();
        const int32_t samplesFromQueue = std::min(numOutputSamples, static_cast<int32_t>(queuedSamples));
        if (samplesFromQueue > 0) {
            // read advances the consumer index internally
            size_t got = mProcessedQueue.read(outputFloats, static_cast<size_t>(samplesFromQueue));
            (void)got; // in normal circumstances got == samplesFromQueue
            // if got < samplesFromQueue, the remaining samples will be cleared below
        }

        if (samplesFromQueue < numOutputSamples) {
            std::fill(outputFloats + samplesFromQueue,
                      outputFloats + numOutputSamples,
                      0.0f);
        }

        // No compaction needed with ring buffer: read() advances and reuses storage.

        if (queueManager && inputFloats) {
            // Pass actual callback-sized output to recorder/analyzer path.
            queueManager->process(const_cast<float *>(inputFloats), outputStart, numOutputSamples);
        }

        return oboe::DataCallbackResult::Continue;
    }
};
#endif //SAMPLES_FULLDUPLEXPASS_H
