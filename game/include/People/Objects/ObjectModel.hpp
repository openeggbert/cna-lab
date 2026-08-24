#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "People/World/LotGrid.hpp"

namespace People::Objects
{
    using ObjectInstanceId = std::uint64_t;

    enum class ObjectCategory : std::uint8_t
    {
        Seating,
        Surfaces,
        Beds,
        Plumbing,
        Appliances,
        Electronics,
        Decoration,
        Recreation,
        Storage,
        Outdoor,
        Miscellaneous
    };

    /** @brief Simulation orientation, deliberately distinct from camera rotation. */
    enum class ObjectRotation : std::uint8_t
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    struct FootprintOffset
    {
        int x = 0;
        int y = 0;

        auto operator<=>(const FootprintOffset&) const = default;
    };

    /** @brief Required resident facing at an interaction target in simulation space. */
    enum class SlotFacing : std::uint8_t
    {
        North = 0,
        East = 1,
        South = 2,
        West = 3
    };

    enum class SlotPosture : std::uint8_t
    {
        Standing,
        Seated,
        Reclining
    };

    /** @brief Authored interaction approach relative to an unrotated object anchor. */
    struct InteractionSlotDefinition
    {
        std::string id;
        FootprintOffset approachOffset;
        SlotFacing facing = SlotFacing::North;
        SlotPosture posture = SlotPosture::Standing;
        std::uint16_t capacity = 1;
        /** @brief Extra offsets relative to the approach tile, not the object anchor. */
        std::vector<FootprintOffset> clearance;
    };

    /** @brief Authored 2D sprite metadata; no texture or renderer handle lives here. */
    struct ObjectSpriteReference
    {
        std::string assetId;
        int anchorX = 0;
        int anchorY = 0;

        bool operator==(const ObjectSpriteReference&) const = default;
    };

    struct DirectionalSpriteSet
    {
        std::array<ObjectSpriteReference, 4> directions;

        bool operator==(const DirectionalSpriteSet&) const = default;
    };

    struct ObjectVisualDefinition
    {
        std::string defaultState = "default";
        std::map<std::string, DirectionalSpriteSet, std::less<>> states;
    };

    /** @brief Immutable simulation/catalog data; it contains no runtime texture. */
    struct ObjectDefinition
    {
        std::string id;
        std::string displayName;
        ObjectCategory category = ObjectCategory::Miscellaneous;
        std::int64_t price = 0;
        std::vector<FootprintOffset> footprint;
        std::vector<FootprintOffset> clearance;
        std::uint8_t allowedRotations = 0x0F;
        ObjectVisualDefinition visual;
        std::vector<InteractionSlotDefinition> interactionSlots;
    };

    /** @brief Persistent mutable identity/placement with no renderer state. */
    struct ObjectInstance
    {
        ObjectInstanceId id = 0;
        std::string definitionId;
        World::TileCoordinate anchor;
        ObjectRotation rotation = ObjectRotation::North;
    };

    class ObjectCatalog final
    {
    public:
        [[nodiscard]] bool Add(ObjectDefinition definition);
        [[nodiscard]] const ObjectDefinition* Find(std::string_view id) const noexcept;
        [[nodiscard]] std::size_t Size() const noexcept;

    private:
        std::map<std::string, ObjectDefinition, std::less<>> definitions_;
    };

    enum class PlacementFailure : std::uint8_t
    {
        None,
        UnknownDefinition,
        InvalidInstanceId,
        DuplicateInstanceId,
        RotationNotAllowed,
        OutsideLot,
        Occupied,
        ClearanceOutsideLot,
        ClearanceBlocked
    };

    struct PlacementResult
    {
        PlacementFailure failure = PlacementFailure::None;
        std::vector<World::TileCoordinate> occupiedCells;
        std::optional<ObjectInstanceId> conflictingInstance;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    enum class SlotResolutionFailure : std::uint8_t
    {
        None,
        UnknownInstance,
        UnknownSlot
    };

    /** @brief Fully rotated simulation target; it contains no presentation state. */
    struct ResolvedInteractionSlot
    {
        ObjectInstanceId objectId = 0;
        std::string slotId;
        World::TileCoordinate approachTile;
        SlotFacing facing = SlotFacing::North;
        SlotPosture posture = SlotPosture::Standing;
        std::uint16_t capacity = 1;
        std::vector<World::TileCoordinate> clearanceTiles;
    };

    struct SlotResolutionResult
    {
        SlotResolutionFailure failure = SlotResolutionFailure::None;
        ResolvedInteractionSlot slot;

        [[nodiscard]] bool IsValid() const noexcept;
    };

    /** @brief Definition-driven placed-object collection and shared validator. */
    class ObjectWorld final
    {
    public:
        explicit ObjectWorld(const World::LotGrid& lot);

        [[nodiscard]] bool RegisterDefinition(ObjectDefinition definition);
        [[nodiscard]] const ObjectCatalog& Catalog() const noexcept;

        [[nodiscard]] PlacementResult ValidatePlacement(
            std::string_view definitionId,
            World::TileCoordinate anchor,
            ObjectRotation rotation) const;
        [[nodiscard]] PlacementResult Place(ObjectInstance instance);
        [[nodiscard]] bool Remove(ObjectInstanceId id);

        [[nodiscard]] const ObjectInstance* Find(ObjectInstanceId id) const noexcept;
        [[nodiscard]] std::vector<World::TileCoordinate> FootprintCells(
            const ObjectInstance& instance) const;
        [[nodiscard]] std::optional<ObjectInstanceId> OccupiedBy(
            World::TileCoordinate tile) const;
        [[nodiscard]] SlotResolutionResult ResolveInteractionSlot(
            ObjectInstanceId objectId, std::string_view slotId) const;
        [[nodiscard]] const std::map<ObjectInstanceId, ObjectInstance>& Instances() const noexcept;

        [[nodiscard]] static FootprintOffset RotateOffset(
            FootprintOffset offset, ObjectRotation rotation);
        [[nodiscard]] static SlotFacing RotateSlotFacing(
            SlotFacing facing, ObjectRotation rotation);
        [[nodiscard]] static std::uint8_t RotationBit(ObjectRotation rotation);

    private:
        [[nodiscard]] std::vector<World::TileCoordinate> ResolveCells(
            const std::vector<FootprintOffset>& offsets,
            World::TileCoordinate anchor,
            ObjectRotation rotation) const;

        const World::LotGrid& lot_;
        ObjectCatalog catalog_;
        std::map<ObjectInstanceId, ObjectInstance> instances_;
        std::map<World::TileCoordinate, ObjectInstanceId> occupancy_;
        std::map<World::TileCoordinate, std::set<ObjectInstanceId>> clearanceClaims_;
    };
}
