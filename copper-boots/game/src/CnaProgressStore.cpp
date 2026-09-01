#include "CopperBoots/CnaProgressStore.hpp"

#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "System/IO/FileAccess.hpp"
#include "System/IO/FileMode.hpp"
#include "System/IO/StreamReader.hpp"
#include "System/IO/StreamWriter.hpp"

namespace CopperBoots
{
    namespace
    {
        constexpr std::string_view SlotAName = "progress-a.cfg";
        constexpr std::string_view SlotBName = "progress-b.cfg";
        constexpr std::string_view ContainerName = "Progress";

        using Microsoft::Xna::Framework::Storage::StorageContainer;
        using Microsoft::Xna::Framework::Storage::StorageDevice;

        struct OpenStorage
        {
            std::unique_ptr<StorageDevice> Device;
            std::unique_ptr<StorageContainer> Container;
        };

        [[nodiscard]] OpenStorage OpenProgressContainer()
        {
            StorageDevice::SetAppNameEXT("CopperBoots");
            auto selector = StorageDevice::BeginShowSelector(nullptr, nullptr);
            auto device = StorageDevice::EndShowSelector(selector.get());
            auto opening = device->BeginOpenContainer(
                std::string(ContainerName), nullptr, nullptr);
            auto container = device->EndOpenContainer(opening.get());
            return {std::move(device), std::move(container)};
        }

        [[nodiscard]] std::optional<std::string> ReadSlot(
            StorageContainer& container, const std::string_view fileName)
        {
            if (!container.FileExists(std::string(fileName)))
                return std::nullopt;
            auto stream = container.OpenFile(
                std::string(fileName), System::IO::FileMode::Open,
                System::IO::FileAccess::Read);
            System::IO::StreamReader reader(stream.get(), true);
            return reader.ReadToEnd();
        }

        [[nodiscard]] std::optional<std::string_view> View(
            const std::optional<std::string>& document) noexcept
        {
            if (!document.has_value())
                return std::nullopt;
            return *document;
        }

        void WriteSlot(StorageContainer& container,
                       const std::string_view fileName,
                       const std::string_view document)
        {
            auto stream = container.CreateFile(std::string(fileName));
            System::IO::StreamWriter writer(stream.get(), true);
            writer.Write(std::string(document));
            writer.Flush();
        }

        [[nodiscard]] ProgressLoadResult LoadFrom(StorageContainer& container)
        {
            const std::optional<std::string> slotA = ReadSlot(
                container, SlotAName);
            const std::optional<std::string> slotB = ReadSlot(
                container, SlotBName);
            return ChooseProgressSlot(View(slotA), View(slotB));
        }
    }

    ProgressLoadResult CnaProgressStore::Load()
    {
        OpenStorage storage = OpenProgressContainer();
        return LoadFrom(*storage.Container);
    }

    ProgressLoadResult CnaProgressStore::Save(const ProgressData& progress)
    {
        OpenStorage storage = OpenProgressContainer();
        const ProgressLoadResult current = LoadFrom(*storage.Container);
        if (current.Generation == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("progress generation is exhausted");
        const std::uint64_t nextGeneration = current.Generation + 1U;
        const ProgressSlot target = NextProgressWriteSlot(current);
        WriteSlot(*storage.Container,
                  target == ProgressSlot::A ? SlotAName : SlotBName,
                  EncodeProgressSlot(progress, nextGeneration));
        return {progress, nextGeneration, target, false};
    }
}
