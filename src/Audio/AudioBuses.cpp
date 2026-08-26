#include "IronGang/Audio/AudioBuses.hpp"

#include <algorithm>

namespace IronGang
{
    namespace
    {
        struct BusDescriptor
        {
            AudioBus bus;
            const char* id;
        };

        constexpr std::array<BusDescriptor, kAudioBusCount> kBuses = {{
            {AudioBus::Master, "master"},
            {AudioBus::Music, "music"},
            {AudioBus::Dialogue, "dialogue"},
            {AudioBus::Ambience, "ambience"},
            {AudioBus::Vehicle, "vehicle"},
            {AudioBus::Effects, "effects"},
            {AudioBus::Ui, "ui"},
        }};
    }

    const char* AudioBusId(AudioBus bus) noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        return index < kAudioBusCount ? kBuses[index].id : "";
    }

    bool ParseAudioBusId(const std::string& id, AudioBus& out) noexcept
    {
        for (const BusDescriptor& descriptor : kBuses)
        {
            if (id == descriptor.id)
            {
                out = descriptor.bus;
                return true;
            }
        }
        return false;
    }

    bool AudioBusIsDuckedByDialogue(AudioBus bus) noexcept
    {
        return bus != AudioBus::Dialogue && bus != AudioBus::Ui && bus != AudioBus::Master;
    }

    AudioBusGraph::AudioBusGraph()
    {
        volumes_.fill(1.0F);
        muted_.fill(false);
    }

    void AudioBusGraph::SetVolume(AudioBus bus, float volume) noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        if (index >= kAudioBusCount)
        {
            return;
        }
        volumes_[index] = std::clamp(volume, 0.0F, 1.0F);
    }

    float AudioBusGraph::GetVolume(AudioBus bus) const noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        return index < kAudioBusCount ? volumes_[index] : 0.0F;
    }

    void AudioBusGraph::SetMuted(AudioBus bus, bool muted) noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        if (index < kAudioBusCount)
        {
            muted_[index] = muted;
        }
    }

    bool AudioBusGraph::IsMuted(AudioBus bus) const noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        return index < kAudioBusCount && muted_[index];
    }

    void AudioBusGraph::Update(float deltaSeconds) noexcept
    {
        if (!(deltaSeconds > 0.0F))
        {
            return;
        }
        const float target = dialogueActive_ ? kDialogueDuckGain : 1.0F;
        // Down fast, up slow: the drop has to be in place before the first syllable, while coming
        // back quickly is what makes a run of short lines pump the whole mix.
        const float seconds = dialogueActive_ ? kDialogueDuckAttackSeconds : kDialogueDuckReleaseSeconds;
        const float span = 1.0F - kDialogueDuckGain;
        const float step = seconds > 0.0F ? (span / seconds) * deltaSeconds : span;
        if (duckGain_ > target)
        {
            duckGain_ = std::max(target, duckGain_ - step);
        }
        else if (duckGain_ < target)
        {
            duckGain_ = std::min(target, duckGain_ + step);
        }
    }

    float AudioBusGraph::GetEffectiveVolume(AudioBus bus, float requestedVolume) const noexcept
    {
        const auto index = static_cast<std::size_t>(bus);
        if (index >= kAudioBusCount)
        {
            return 0.0F;
        }
        if (muted_[index] || muted_[static_cast<std::size_t>(AudioBus::Master)])
        {
            return 0.0F;
        }
        float gain = std::clamp(requestedVolume, 0.0F, 1.0F) * volumes_[index];
        if (bus != AudioBus::Master)
        {
            gain *= volumes_[static_cast<std::size_t>(AudioBus::Master)];
        }
        if (AudioBusIsDuckedByDialogue(bus))
        {
            gain *= duckGain_;
        }
        return std::clamp(gain, 0.0F, 1.0F);
    }
}
