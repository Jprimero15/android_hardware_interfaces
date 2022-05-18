/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <aidl/android/media/audio/common/AudioChannelLayout.h>
#include <aidl/android/media/audio/common/AudioDeviceType.h>
#include <aidl/android/media/audio/common/AudioFormatType.h>
#include <aidl/android/media/audio/common/AudioIoFlags.h>
#include <aidl/android/media/audio/common/AudioOutputFlags.h>

#include "aidl/android/media/audio/common/AudioFormatDescription.h"
#include "core-impl/Configuration.h"

using aidl::android::media::audio::common::AudioChannelLayout;
using aidl::android::media::audio::common::AudioDeviceType;
using aidl::android::media::audio::common::AudioFormatDescription;
using aidl::android::media::audio::common::AudioFormatType;
using aidl::android::media::audio::common::AudioGainConfig;
using aidl::android::media::audio::common::AudioIoFlags;
using aidl::android::media::audio::common::AudioOutputFlags;
using aidl::android::media::audio::common::AudioPort;
using aidl::android::media::audio::common::AudioPortConfig;
using aidl::android::media::audio::common::AudioPortDeviceExt;
using aidl::android::media::audio::common::AudioPortExt;
using aidl::android::media::audio::common::AudioPortMixExt;
using aidl::android::media::audio::common::AudioProfile;
using aidl::android::media::audio::common::Int;
using aidl::android::media::audio::common::PcmType;

namespace aidl::android::hardware::audio::core::internal {

static AudioProfile createProfile(PcmType pcmType, const std::vector<int32_t>& channelLayouts,
                                  const std::vector<int32_t>& sampleRates) {
    AudioProfile profile;
    profile.format.type = AudioFormatType::PCM;
    profile.format.pcm = pcmType;
    for (auto layout : channelLayouts) {
        profile.channelMasks.push_back(
                AudioChannelLayout::make<AudioChannelLayout::layoutMask>(layout));
    }
    profile.sampleRates.insert(profile.sampleRates.end(), sampleRates.begin(), sampleRates.end());
    return profile;
}

static AudioPortExt createDeviceExt(AudioDeviceType devType, int32_t flags) {
    AudioPortDeviceExt deviceExt;
    deviceExt.device.type.type = devType;
    deviceExt.flags = flags;
    return AudioPortExt::make<AudioPortExt::Tag::device>(deviceExt);
}

static AudioPortExt createPortMixExt(int32_t maxOpenStreamCount, int32_t maxActiveStreamCount) {
    AudioPortMixExt mixExt;
    mixExt.maxOpenStreamCount = maxOpenStreamCount;
    mixExt.maxActiveStreamCount = maxActiveStreamCount;
    return AudioPortExt::make<AudioPortExt::Tag::mix>(mixExt);
}

static AudioPort createPort(int32_t id, const std::string& name, int32_t flags, bool isInput,
                            const AudioPortExt& ext) {
    AudioPort port;
    port.id = id;
    port.name = name;
    port.flags = isInput ? AudioIoFlags::make<AudioIoFlags::Tag::input>(flags)
                         : AudioIoFlags::make<AudioIoFlags::Tag::output>(flags);
    port.ext = ext;
    return port;
}

static AudioPortConfig createPortConfig(int32_t id, int32_t portId, PcmType pcmType, int32_t layout,
                                        int32_t sampleRate, int32_t flags, bool isInput,
                                        const AudioPortExt& ext) {
    AudioPortConfig config;
    config.id = id;
    config.portId = portId;
    config.sampleRate = Int{.value = sampleRate};
    config.channelMask = AudioChannelLayout::make<AudioChannelLayout::layoutMask>(layout);
    config.format = AudioFormatDescription{.type = AudioFormatType::PCM, .pcm = pcmType};
    config.gain = AudioGainConfig();
    config.flags = isInput ? AudioIoFlags::make<AudioIoFlags::Tag::input>(flags)
                           : AudioIoFlags::make<AudioIoFlags::Tag::output>(flags);
    config.ext = ext;
    return config;
}

static AudioRoute createRoute(const std::vector<int32_t>& sources, int32_t sink) {
    AudioRoute route;
    route.sinkPortId = sink;
    route.sourcePortIds.insert(route.sourcePortIds.end(), sources.begin(), sources.end());
    return route;
}

Configuration& getNullPrimaryConfiguration() {
    static Configuration configuration = []() {
        Configuration c;

        AudioPort nullOutDevice =
                createPort(c.nextPortId++, "Null", 0, false,
                           createDeviceExt(AudioDeviceType::OUT_SPEAKER,
                                           1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE));
        c.ports.push_back(nullOutDevice);

        AudioPort primaryOutMix = createPort(c.nextPortId++, "primary output",
                                             1 << static_cast<int32_t>(AudioOutputFlags::PRIMARY),
                                             false, createPortMixExt(1, 1));
        primaryOutMix.profiles.push_back(
                createProfile(PcmType::INT_16_BIT,
                              {AudioChannelLayout::LAYOUT_MONO, AudioChannelLayout::LAYOUT_STEREO},
                              {44100, 48000}));
        primaryOutMix.profiles.push_back(
                createProfile(PcmType::INT_24_BIT,
                              {AudioChannelLayout::LAYOUT_MONO, AudioChannelLayout::LAYOUT_STEREO},
                              {44100, 48000}));
        c.ports.push_back(primaryOutMix);

        c.routes.push_back(createRoute({primaryOutMix.id}, nullOutDevice.id));

        c.initialConfigs.push_back(
                createPortConfig(nullOutDevice.id, nullOutDevice.id, PcmType::INT_24_BIT,
                                 AudioChannelLayout::LAYOUT_STEREO, 48000, 0, false,
                                 createDeviceExt(AudioDeviceType::OUT_SPEAKER, 0)));

        AudioPort loopOutDevice = createPort(c.nextPortId++, "Loopback Out", 0, false,
                                             createDeviceExt(AudioDeviceType::OUT_SUBMIX, 0));
        loopOutDevice.profiles.push_back(
                createProfile(PcmType::INT_24_BIT, {AudioChannelLayout::LAYOUT_STEREO}, {48000}));
        c.ports.push_back(loopOutDevice);

        AudioPort loopOutMix =
                createPort(c.nextPortId++, "loopback output", 0, false, createPortMixExt(0, 0));
        loopOutMix.profiles.push_back(
                createProfile(PcmType::INT_24_BIT, {AudioChannelLayout::LAYOUT_STEREO}, {48000}));
        c.ports.push_back(loopOutMix);

        c.routes.push_back(createRoute({loopOutMix.id}, loopOutDevice.id));

        AudioPort zeroInDevice =
                createPort(c.nextPortId++, "Zero", 0, true,
                           createDeviceExt(AudioDeviceType::IN_MICROPHONE,
                                           1 << AudioPortDeviceExt::FLAG_INDEX_DEFAULT_DEVICE));
        c.ports.push_back(zeroInDevice);

        AudioPort primaryInMix =
                createPort(c.nextPortId++, "primary input", 0, true, createPortMixExt(2, 2));
        primaryInMix.profiles.push_back(
                createProfile(PcmType::INT_16_BIT,
                              {AudioChannelLayout::LAYOUT_MONO, AudioChannelLayout::LAYOUT_STEREO,
                               AudioChannelLayout::LAYOUT_FRONT_BACK},
                              {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000}));
        primaryInMix.profiles.push_back(
                createProfile(PcmType::INT_24_BIT,
                              {AudioChannelLayout::LAYOUT_MONO, AudioChannelLayout::LAYOUT_STEREO,
                               AudioChannelLayout::LAYOUT_FRONT_BACK},
                              {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000}));
        c.ports.push_back(primaryInMix);

        c.routes.push_back(createRoute({zeroInDevice.id}, primaryInMix.id));

        c.initialConfigs.push_back(
                createPortConfig(zeroInDevice.id, zeroInDevice.id, PcmType::INT_24_BIT,
                                 AudioChannelLayout::LAYOUT_MONO, 48000, 0, true,
                                 createDeviceExt(AudioDeviceType::IN_MICROPHONE, 0)));

        AudioPort loopInDevice = createPort(c.nextPortId++, "Loopback In", 0, true,
                                            createDeviceExt(AudioDeviceType::IN_SUBMIX, 0));
        loopInDevice.profiles.push_back(
                createProfile(PcmType::INT_24_BIT, {AudioChannelLayout::LAYOUT_STEREO}, {48000}));
        c.ports.push_back(loopInDevice);

        AudioPort loopInMix =
                createPort(c.nextPortId++, "loopback input", 0, true, createPortMixExt(0, 0));
        loopInMix.profiles.push_back(
                createProfile(PcmType::INT_24_BIT, {AudioChannelLayout::LAYOUT_STEREO}, {48000}));
        c.ports.push_back(loopInMix);

        c.routes.push_back(createRoute({loopInDevice.id}, loopInMix.id));

        c.portConfigs.insert(c.portConfigs.end(), c.initialConfigs.begin(), c.initialConfigs.end());
        return c;
    }();
    return configuration;
}

}  // namespace aidl::android::hardware::audio::core::internal
