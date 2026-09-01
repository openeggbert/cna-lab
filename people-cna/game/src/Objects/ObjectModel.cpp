#include "People/Objects/ObjectModel.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace People::Objects
{
    namespace
    {
        void ValidateOffsets(
            const std::vector<FootprintOffset>& offsets,
            const bool requireNonEmpty,
            const char* field)
        {
            if (requireNonEmpty && offsets.empty())
                throw std::invalid_argument(std::string(field) + " must not be empty");
            const std::set<FootprintOffset> unique(offsets.begin(), offsets.end());
            if (unique.size() != offsets.size())
                throw std::invalid_argument(std::string(field) + " contains duplicate cells");
        }

        void ValidateDefinition(const ObjectDefinition& definition)
        {
            if (definition.id.empty())
                throw std::invalid_argument("object definition ID must not be empty");
            if (definition.displayName.empty())
                throw std::invalid_argument("object display name must not be empty");
            if (definition.price < 0)
                throw std::invalid_argument("object price must not be negative");
            if ((definition.allowedRotations & 0x0F) == 0
                || (definition.allowedRotations & 0xF0) != 0)
                throw std::invalid_argument("object allowed-rotation mask must use four direction bits");
            ValidateOffsets(definition.footprint, true, "object footprint");
            ValidateOffsets(definition.clearance, false, "object clearance");

            const std::set<FootprintOffset> footprint(
                definition.footprint.begin(), definition.footprint.end());
            for (const FootprintOffset clearance : definition.clearance)
            {
                if (footprint.contains(clearance))
                    throw std::invalid_argument("object clearance overlaps its footprint");
            }

            std::set<std::string> slotIds;
            for (const InteractionSlotDefinition& slot : definition.interactionSlots)
            {
                if (slot.id.empty())
                    throw std::invalid_argument("interaction slot ID must not be empty");
                if (!slotIds.insert(slot.id).second)
                    throw std::invalid_argument("object contains a duplicate interaction slot ID");
                if (slot.capacity == 0)
                    throw std::invalid_argument("interaction slot capacity must be positive");
                switch (slot.facing)
                {
                    case SlotFacing::North:
                    case SlotFacing::East:
                    case SlotFacing::South:
                    case SlotFacing::West: break;
                    default: throw std::invalid_argument("interaction slot facing is invalid");
                }
                switch (slot.posture)
                {
                    case SlotPosture::Standing:
                    case SlotPosture::Seated:
                    case SlotPosture::Reclining: break;
                    default: throw std::invalid_argument("interaction slot posture is invalid");
                }
                ValidateOffsets(slot.clearance, false, "interaction slot clearance");
            }

            if (!definition.visual.states.empty())
            {
                if (definition.visual.defaultState.empty()
                    || !definition.visual.states.contains(definition.visual.defaultState))
                {
                    throw std::invalid_argument(
                        "object default visual state must reference an authored state");
                }
                for (const auto& [state, spriteSet] : definition.visual.states)
                {
                    if (state.empty())
                        throw std::invalid_argument("object visual state ID must not be empty");
                    for (const ObjectSpriteReference& sprite : spriteSet.directions)
                    {
                        if (sprite.assetId.empty())
                        {
                            throw std::invalid_argument(
                                "every object visual direction requires an asset ID");
                        }
                    }
                }
            }
        }
    }

    bool ObjectCatalog::Add(ObjectDefinition definition)
    {
        ValidateDefinition(definition);
        const std::string id = definition.id;
        return definitions_.emplace(id, std::move(definition)).second;
    }

    const ObjectDefinition* ObjectCatalog::Find(const std::string_view id) const noexcept
    {
        const auto found = definitions_.find(id);
        return found == definitions_.end() ? nullptr : &found->second;
    }

    std::size_t ObjectCatalog::Size() const noexcept
    {
        return definitions_.size();
    }

    bool PlacementResult::IsValid() const noexcept
    {
        return failure == PlacementFailure::None;
    }

    bool SlotResolutionResult::IsValid() const noexcept
    {
        return failure == SlotResolutionFailure::None;
    }

    ObjectWorld::ObjectWorld(const World::LotGrid& lot) : lot_(lot)
    {
    }

    bool ObjectWorld::RegisterDefinition(ObjectDefinition definition)
    {
        return catalog_.Add(std::move(definition));
    }

    const ObjectCatalog& ObjectWorld::Catalog() const noexcept
    {
        return catalog_;
    }

    FootprintOffset ObjectWorld::RotateOffset(
        const FootprintOffset offset, const ObjectRotation rotation)
    {
        switch (rotation)
        {
            case ObjectRotation::North: return offset;
            case ObjectRotation::East: return {-offset.y, offset.x};
            case ObjectRotation::South: return {-offset.x, -offset.y};
            case ObjectRotation::West: return {offset.y, -offset.x};
        }
        throw std::invalid_argument("object rotation must be one of four directions");
    }

    std::uint8_t ObjectWorld::RotationBit(const ObjectRotation rotation)
    {
        const auto value = static_cast<std::uint8_t>(rotation);
        if (value > 3)
            throw std::invalid_argument("object rotation must be one of four directions");
        return static_cast<std::uint8_t>(1U << value);
    }

    SlotFacing ObjectWorld::RotateSlotFacing(
        const SlotFacing facing, const ObjectRotation rotation)
    {
        const auto facingValue = static_cast<std::uint8_t>(facing);
        const auto rotationValue = static_cast<std::uint8_t>(rotation);
        if (facingValue > 3)
            throw std::invalid_argument("interaction slot facing must be cardinal");
        if (rotationValue > 3)
            throw std::invalid_argument("object rotation must be one of four directions");
        return static_cast<SlotFacing>((facingValue + rotationValue) % 4U);
    }

    std::vector<World::TileCoordinate> ObjectWorld::ResolveCells(
        const std::vector<FootprintOffset>& offsets,
        const World::TileCoordinate anchor,
        const ObjectRotation rotation) const
    {
        std::vector<World::TileCoordinate> cells;
        cells.reserve(offsets.size());
        for (const FootprintOffset offset : offsets)
        {
            const FootprintOffset rotated = RotateOffset(offset, rotation);
            cells.push_back({anchor.x + rotated.x, anchor.y + rotated.y, anchor.floor});
        }
        return cells;
    }

    PlacementResult ObjectWorld::ValidatePlacement(
        const std::string_view definitionId,
        const World::TileCoordinate anchor,
        const ObjectRotation rotation) const
    {
        const ObjectDefinition* definition = catalog_.Find(definitionId);
        if (definition == nullptr)
            return {PlacementFailure::UnknownDefinition, {}, std::nullopt};
        if ((definition->allowedRotations & RotationBit(rotation)) == 0)
            return {PlacementFailure::RotationNotAllowed, {}, std::nullopt};

        std::vector<World::TileCoordinate> occupied = ResolveCells(
            definition->footprint, anchor, rotation);
        for (const World::TileCoordinate cell : occupied)
        {
            if (!lot_.Contains(cell))
                return {PlacementFailure::OutsideLot, std::move(occupied), std::nullopt};
            const auto conflict = occupancy_.find(cell);
            if (conflict != occupancy_.end())
                return {PlacementFailure::Occupied, std::move(occupied), conflict->second};
            const auto clearanceConflict = clearanceClaims_.find(cell);
            if (clearanceConflict != clearanceClaims_.end()
                && !clearanceConflict->second.empty())
            {
                return {PlacementFailure::ClearanceBlocked, std::move(occupied),
                        *clearanceConflict->second.begin()};
            }
        }

        const std::vector<World::TileCoordinate> clearance = ResolveCells(
            definition->clearance, anchor, rotation);
        for (const World::TileCoordinate cell : clearance)
        {
            if (!lot_.Contains(cell))
                return {PlacementFailure::ClearanceOutsideLot, std::move(occupied), std::nullopt};
            const auto conflict = occupancy_.find(cell);
            if (conflict != occupancy_.end())
                return {PlacementFailure::ClearanceBlocked, std::move(occupied), conflict->second};
        }
        return {PlacementFailure::None, std::move(occupied), std::nullopt};
    }

    PlacementResult ObjectWorld::Place(ObjectInstance instance)
    {
        if (instance.id == 0)
            return {PlacementFailure::InvalidInstanceId, {}, std::nullopt};
        if (instances_.contains(instance.id))
            return {PlacementFailure::DuplicateInstanceId, {}, instance.id};

        PlacementResult result = ValidatePlacement(
            instance.definitionId, instance.anchor, instance.rotation);
        if (!result.IsValid())
            return result;

        for (const World::TileCoordinate cell : result.occupiedCells)
            occupancy_.emplace(cell, instance.id);
        const ObjectDefinition* definition = catalog_.Find(instance.definitionId);
        if (definition == nullptr)
            throw std::logic_error("validated object definition disappeared");
        const std::vector<World::TileCoordinate> clearance = ResolveCells(
            definition->clearance, instance.anchor, instance.rotation);
        for (const World::TileCoordinate cell : clearance)
            clearanceClaims_[cell].insert(instance.id);
        instances_.emplace(instance.id, std::move(instance));
        return result;
    }

    bool ObjectWorld::Remove(const ObjectInstanceId id)
    {
        const auto found = instances_.find(id);
        if (found == instances_.end())
            return false;
        const ObjectDefinition* definition = catalog_.Find(found->second.definitionId);
        if (definition == nullptr)
            throw std::logic_error("placed object refers to missing definition");
        const std::vector<World::TileCoordinate> cells = ResolveCells(
            definition->footprint, found->second.anchor, found->second.rotation);
        for (const World::TileCoordinate cell : cells)
            occupancy_.erase(cell);
        const std::vector<World::TileCoordinate> clearance = ResolveCells(
            definition->clearance, found->second.anchor, found->second.rotation);
        for (const World::TileCoordinate cell : clearance)
        {
            const auto claim = clearanceClaims_.find(cell);
            if (claim == clearanceClaims_.end())
                throw std::logic_error("placed object clearance claim is missing");
            claim->second.erase(id);
            if (claim->second.empty())
                clearanceClaims_.erase(claim);
        }
        instances_.erase(found);
        return true;
    }

    const ObjectInstance* ObjectWorld::Find(const ObjectInstanceId id) const noexcept
    {
        const auto found = instances_.find(id);
        return found == instances_.end() ? nullptr : &found->second;
    }

    std::vector<World::TileCoordinate> ObjectWorld::FootprintCells(
        const ObjectInstance& instance) const
    {
        const ObjectDefinition* definition = catalog_.Find(instance.definitionId);
        if (definition == nullptr)
            throw std::invalid_argument("object instance refers to an unknown definition");
        return ResolveCells(definition->footprint, instance.anchor, instance.rotation);
    }

    std::optional<ObjectInstanceId> ObjectWorld::OccupiedBy(
        const World::TileCoordinate tile) const
    {
        const auto found = occupancy_.find(tile);
        return found == occupancy_.end()
            ? std::nullopt : std::optional<ObjectInstanceId>(found->second);
    }

    SlotResolutionResult ObjectWorld::ResolveInteractionSlot(
        const ObjectInstanceId objectId, const std::string_view slotId) const
    {
        const ObjectInstance* instance = Find(objectId);
        if (instance == nullptr)
            return {SlotResolutionFailure::UnknownInstance, {}};
        const ObjectDefinition* definition = catalog_.Find(instance->definitionId);
        if (definition == nullptr)
            throw std::logic_error("placed object refers to missing definition");

        const auto found = std::find_if(
            definition->interactionSlots.begin(), definition->interactionSlots.end(),
            [slotId](const InteractionSlotDefinition& slot) { return slot.id == slotId; });
        if (found == definition->interactionSlots.end())
            return {SlotResolutionFailure::UnknownSlot, {}};

        const FootprintOffset rotatedApproach = RotateOffset(
            found->approachOffset, instance->rotation);
        const World::TileCoordinate approach{
            instance->anchor.x + rotatedApproach.x,
            instance->anchor.y + rotatedApproach.y,
            instance->anchor.floor
        };
        ResolvedInteractionSlot resolved{
            objectId,
            found->id,
            approach,
            RotateSlotFacing(found->facing, instance->rotation),
            found->posture,
            found->capacity,
            {}
        };
        resolved.clearanceTiles.reserve(found->clearance.size());
        for (const FootprintOffset offset : found->clearance)
        {
            const FootprintOffset rotated = RotateOffset(offset, instance->rotation);
            resolved.clearanceTiles.push_back({
                approach.x + rotated.x,
                approach.y + rotated.y,
                approach.floor
            });
        }
        return {SlotResolutionFailure::None, std::move(resolved)};
    }

    const std::map<ObjectInstanceId, ObjectInstance>& ObjectWorld::Instances() const noexcept
    {
        return instances_;
    }
}
