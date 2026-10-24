// SPDX-License-Identifier: MIT
#pragma once
#import <AudioToolbox/AudioToolbox.h>
#include <deque>
#include <mutex>
#include <vector>
#include <cstdint>
// The emulation and UI run on the main thread; only PCM crosses to AudioQueue.
struct AudioOut {
    AudioQueueRef queue = nullptr;
    std::mutex mutex;
    std::deque<int16_t> samples;
    size_t capacity = 44100;
    static void callback(void* context, AudioQueueRef queue, AudioQueueBufferRef buffer) {
        auto* self = static_cast<AudioOut*>(context);
        auto* dst = static_cast<int16_t*>(buffer->mAudioData);
        size_t count = buffer->mAudioDataBytesCapacity / sizeof(int16_t);
        {
            std::lock_guard<std::mutex> lock(self->mutex);
            for (size_t i = 0; i < count; ++i) {
                dst[i] = self->samples.empty() ? 0 : self->samples.front();
                if (!self->samples.empty()) self->samples.pop_front();
            }
        }
        buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
        AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
    }
    bool open(int rate) {
        close();
        capacity = (size_t)rate / 2; // at most 250 ms of stereo PCM
        AudioStreamBasicDescription fmt = {};
        fmt.mSampleRate = rate;
        fmt.mFormatID = kAudioFormatLinearPCM;
        fmt.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
        fmt.mBytesPerPacket = fmt.mBytesPerFrame = 4;
        fmt.mFramesPerPacket = 1; fmt.mChannelsPerFrame = 2; fmt.mBitsPerChannel = 16;
        if (AudioQueueNewOutput(&fmt, callback, this, nullptr, nullptr, 0, &queue) != noErr) return false;
        for (int i = 0; i < 3; ++i) {
            AudioQueueBufferRef b;
            if (AudioQueueAllocateBuffer(queue, 512 * 4, &b) != noErr) { close(); return false; }
            callback(this, queue, b);
        }
        if (AudioQueueStart(queue, nullptr) != noErr) { close(); return false; }
        return true;
    }
    void push(const std::vector<int16_t>& pcm) {
        if (!queue) return;
        std::lock_guard<std::mutex> lock(mutex);
        if (samples.size() + pcm.size() > capacity) samples.clear();
        samples.insert(samples.end(), pcm.begin(), pcm.end());
    }
    void clear() { std::lock_guard<std::mutex> lock(mutex); samples.clear(); }
    void close() {
        if (queue) { AudioQueueStop(queue, true); AudioQueueDispose(queue, true); queue = nullptr; }
        clear();
    }
    ~AudioOut() { close(); }
};
