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
     * @brief Configure how many blocks of processed audio the internal ring buffer
     * should hold. Each "block" is `mFixedBlockSamples` floats. Default is 4 blocks.
     *
     * Call before opening streams or it will take effect on the next reinitialization
     * of the block adapter.
     */
    void setProcessedQueueBlocks(size_t blocks) {
        mRequestedProcessedBlocks = (blocks > 0) ? blocks : 1;
    }

    /**
     * @brief Return the number of processed blocks that were dropped due to a full ring buffer.
     */
    uint32_t getDroppedProcessedBlocks() const {
        return droppedProcessedBlocks.load(std::memory_order_relaxed);
    }

    // Runtime timestamp-based latency estimate (one-way: input - output)
    std::atomic<int64_t> lastTimestampLatencyFrames{0};   ///< last measured frame difference (inputFrame - outputFrame)
    std::atomic<double>  lastTimestampLatencyMs{0.0};     ///< last measured latency in milliseconds

    // Return latest timestamp-based estimated one-way latency in milliseconds.
    double getEstimatedLatencyMs() const {
        return lastTimestampLatencyMs.load(std::memory_order_relaxed);
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
    size_t mRequestedProcessedBlocks = 4; ///< Preferred number of blocks to allocate for processed queue.

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
        // Default capacity: use the requested number of blocks (each block = mFixedBlockSamples)
        // or fall back to a small default (4 blocks). Ensure at least 2 blocks of capacity.
        size_t blocks = (mRequestedProcessedBlocks > 0) ? mRequestedProcessedBlocks : 4;
        size_t requestedCapacity = static_cast<size_t>(mFixedBlockSamples) * blocks;
        requestedCapacity = std::max<size_t>(requestedCapacity, static_cast<size_t>(mFixedBlockSamples * 2));
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

        // begin low latency manual patch
        memcpy(outputFloats, inputFloats, static_cast<size_t>(numOutputSamples) * sizeof(float));
        for (int x = 0 ; x < plugins.size() ; x++) {
            LV2Plugin* p = plugins[x];
            if (p && p->enabled) p->process(outputFloats, outputFloats, numOutputSamples);
        }

        return oboe::DataCallbackResult::Continue;

        // old code (unchanged)
        int32_t inputReadIndex = 0;
        const float gainValue = gain ? gain->load(std::memory_order_relaxed) : 1.0f;

        // Stage B (optimised): first drain any previously processed samples from the
        // ring buffer into the start of the output buffer. This frees us to write
        // newly processed blocks directly into the output buffer while there is
        // available space, avoiding an extra ring hop.
        int32_t outputWriteIndex = 0;
        const size_t queuedSamples = mProcessedQueue.size();
        const int32_t initialFromQueue = std::min(numOutputSamples, static_cast<int32_t>(queuedSamples));
        if (initialFromQueue > 0) {
            size_t got = mProcessedQueue.read(outputFloats, static_cast<size_t>(initialFromQueue));
            outputWriteIndex = static_cast<int32_t>(got);
        }

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

                // If there is still space left in the current output buffer, write
                // the processed block directly to output (no ring hop). Otherwise
                // push the full block into the ring buffer for later callbacks.
                if (outputWriteIndex + mFixedBlockSamples <= numOutputSamples) {
                    std::memcpy(outputFloats + outputWriteIndex, mOutputBlock.data(),
                                static_cast<size_t>(mFixedBlockSamples) * sizeof(float));
                    outputWriteIndex += mFixedBlockSamples;
                } else {
                    bool pushed = mProcessedQueue.push(mOutputBlock.data(), static_cast<size_t>(mFixedBlockSamples));
                    if (!pushed) {
                        droppedProcessedBlocks.fetch_add(1u, std::memory_order_relaxed);
                    }
                }
                mInputFillSamples = 0;
            }
        }

        // Fill any remaining output frames with silence.
        if (outputWriteIndex < numOutputSamples) {
            std::fill(outputFloats + outputWriteIndex,
                      outputFloats + numOutputSamples,
                      0.0f);
        }

        // No compaction needed with ring buffer: read() advances and reuses storage.

        if (queueManager && inputFloats) {
            // Pass actual callback-sized output to recorder/analyzer path.
            queueManager->process(const_cast<float *>(inputFloats), outputStart, numOutputSamples);
        }

        // Sample Oboe timestamps for output and input streams and compute one-way latency.
        // We call getTimestamp() on both streams while in the callback; the calls are cheap.
        if (getOutputStream() && getInputStream()) {
            int64_t outFrame = 0, outTimeNanos = 0;
            int64_t inFrame = 0, inTimeNanos = 0;
            if (getOutputStream()->getTimestamp(CLOCK_MONOTONIC, &outFrame, &outTimeNanos) == oboe::Result::OK &&
                getInputStream()->getTimestamp(CLOCK_MONOTONIC, &inFrame, &inTimeNanos) == oboe::Result::OK) {
                int64_t latencyFrames = inFrame - outFrame; // input frame position minus output frame pos
                double sampleRate = static_cast<double>(getOutputStream()->getSampleRate());
                double latencyMs = 0.0;
                if (sampleRate > 0.0) {
                    latencyMs = (static_cast<double>(latencyFrames) / sampleRate) * 1000.0;
                }
                lastTimestampLatencyFrames.store(latencyFrames, std::memory_order_relaxed);
                lastTimestampLatencyMs.store(latencyMs, std::memory_order_relaxed);
            }
        }

        return oboe::DataCallbackResult::Continue;
    }
};
#endif //SAMPLES_FULLDUPLEXPASS_H
