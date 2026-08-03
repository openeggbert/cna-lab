// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Assets/AssetCommands.hpp"

namespace CNA::Editor
{
    MoveAssetCommand::MoveAssetCommand(AssetDatabase& assets, Uuid assetId, std::string newRelativePath)
        : assets_(&assets), assetId_(assetId), newPath_(std::move(newRelativePath))
    {
        const AssetRecord* record = assets_->find(assetId_);
        if (record == nullptr)
        {
            error_ = "no asset with that id";
            return;
        }

        oldPath_ = record->sourcePath;
        if (oldPath_ == newPath_)
        {
            error_ = "the asset is already there";
            return;
        }
        if (newPath_.empty())
        {
            error_ = "destination path is empty";
            return;
        }

        valid_ = true;
    }

    void MoveAssetCommand::execute()
    {
        if (!valid_) { return; }
        if (!assets_->moveAsset(assetId_, newPath_, &error_))
        {
            // A move that fails leaves the asset exactly where it was, so the command is retired
            // rather than left in a state where undo would move something it never moved.
            valid_ = false;
        }
    }

    void MoveAssetCommand::undo()
    {
        if (!valid_) { return; }
        assets_->moveAsset(assetId_, oldPath_, &error_);
    }

    std::string MoveAssetCommand::getDescription() const
    {
        return "Move '" + oldPath_ + "' to '" + newPath_ + "'";
    }

    SetImporterSettingCommand::SetImporterSettingCommand(AssetDatabase& assets,
                                                         Uuid assetId,
                                                         std::string settingName,
                                                         PropertyValue newValue)
        : assets_(&assets),
          assetId_(assetId),
          settingName_(std::move(settingName)),
          newValue_(std::move(newValue))
    {
        const AssetRecord* record = assets_->find(assetId_);
        if (record == nullptr) { return; }

        // Absent and present-but-default are different states: the first must undo back to absent
        // so the sidecar does not grow a field the user never set.
        const JsonValue& stored = record->importerSettings[settingName_];
        if (!stored.isNull())
        {
            oldValue_ = PropertyValue::fromJson(stored, newValue_.getType());
            hadOldValue_ = true;
        }

        valid_ = true;
    }

    void SetImporterSettingCommand::apply(const PropertyValue& value) const
    {
        AssetRecord* record = assets_->findMutable(assetId_);
        if (record == nullptr) { return; }

        if (record->importerSettings.isNull()) { record->importerSettings = JsonValue::makeObject(); }
        record->importerSettings.set(settingName_, value.toJson());

        // Written through immediately. An import setting that lived only in memory would be lost
        // on the next scan, and the user would have no way to tell that from it having no effect.
        assets_->writeSidecar(assetId_);
    }

    void SetImporterSettingCommand::execute()
    {
        if (!valid_) { return; }
        apply(newValue_);
    }

    void SetImporterSettingCommand::undo()
    {
        if (!valid_) { return; }

        if (hadOldValue_)
        {
            apply(oldValue_);
            return;
        }

        // The setting was absent before, so undo removes it rather than writing a default. A
        // sidecar that accumulated every field the user ever glanced at would make every asset's
        // diff noise.
        if (AssetRecord* record = assets_->findMutable(assetId_); record != nullptr)
        {
            record->importerSettings.remove(settingName_);
            assets_->writeSidecar(assetId_);
        }
    }

    std::string SetImporterSettingCommand::getDescription() const
    {
        return "Set import setting '" + settingName_ + "'";
    }

    std::string SetImporterSettingCommand::getMergeKey() const
    {
        return "importer:" + assetId_.toString() + ":" + settingName_;
    }

    bool SetImporterSettingCommand::mergeWith(const EditorCommand& newer)
    {
        const auto* other = dynamic_cast<const SetImporterSettingCommand*>(&newer);
        if (other == nullptr || other->assetId_ != assetId_ || other->settingName_ != settingName_)
        {
            return false;
        }

        // Keep this command's original value (the undo target) and adopt the newer final one.
        newValue_ = other->newValue_;
        return true;
    }
}
