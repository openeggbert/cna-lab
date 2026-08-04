// SPDX-License-Identifier: MS-PL
/**
 * @file CnaEditorAudio.cpp
 * @brief The CNA-backed audio preview.
 *
 * One clip at a time, loaded straight from the asset's own file through CNA's public
 * `SoundEffect(path)` constructor -- no content pipeline, no `.xnb`, and therefore nothing this
 * editor is forbidden from touching (D-01).
 */

#include "CNA/Editor/Viewport/EditorAudio.hpp"

#include <exception>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "CNA/Editor/Assets/AssetDatabase.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"

namespace CNA::Editor
{
    namespace
    {
        namespace XnaAudio = Microsoft::Xna::Framework::Audio;
    }

    /** @brief Plays one preview at a time through CNA. */
    class CnaEditorAudio final : public EditorAudio
    {
    public:
        explicit CnaEditorAudio(const AssetDatabase& assets) : assets_(&assets) {}

        [[nodiscard]] const char* getBackendName() const override { return "cna"; }

        bool play(const Uuid& assetId, float volume, float pitch, float pan) override
        {
            stop();

            const AssetRecord* record = assets_->find(assetId);
            if (record == nullptr) { return false; }

            XnaAudio::SoundEffect* effect = load(assetId, assets_->resolvePath(record->sourcePath));
            if (effect == nullptr) { return false; }

            try
            {
                // XNA's own parameter order and ranges, taken straight from the component being
                // previewed, so what the editor plays is what the game will play.
                playing_ = effect->Play(volume, pitch, pan);
            }
            catch (const std::exception&)
            {
                // A device that refuses a clip is a report, not a crash: an editor that died
                // because a preview would not play would be worse than one that stays quiet.
                playing_ = false;
            }
            return playing_;
        }

        void stop() override
        {
            // CNA's fire-and-forget Play() hands back no handle, so there is nothing to stop but
            // the editor's own belief about it. Tracking a SoundEffectInstance instead would let
            // the editor stop a preview, and is the shape to reach for when someone asks for it.
            playing_ = false;
        }

        [[nodiscard]] bool isPlaying() const override { return playing_; }

    private:
        /** @brief Returns the loaded effect for @p assetId, loading it once. */
        XnaAudio::SoundEffect* load(const Uuid& assetId, const std::string& path)
        {
            if (const auto found = effects_.find(assetId); found != effects_.end())
            {
                return found->second.get();
            }
            if (failed_.count(assetId) > 0) { return nullptr; }

            try
            {
                auto effect = std::make_unique<XnaAudio::SoundEffect>(path);
                XnaAudio::SoundEffect* raw = effect.get();
                effects_.emplace(assetId, std::move(effect));
                return raw;
            }
            catch (const std::exception&)
            {
                // Remembered as failed, so a broken clip costs one attempt rather than one per
                // press -- the same rule the scene renderer's texture cache follows.
                failed_.insert(assetId);
                return nullptr;
            }
        }

        const AssetDatabase* assets_;
        std::unordered_map<Uuid, std::unique_ptr<XnaAudio::SoundEffect>> effects_;
        std::unordered_set<Uuid> failed_;
        bool playing_ = false;
    };

    std::unique_ptr<EditorAudio> createCnaEditorAudio(const AssetDatabase& assets)
    {
        return std::make_unique<CnaEditorAudio>(assets);
    }
}
