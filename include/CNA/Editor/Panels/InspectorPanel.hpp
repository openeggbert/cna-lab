// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Panels/InspectorPanel.hpp
 * @brief The property editor for the primary selection.
 */

#include <optional>
#include <string>

#include "CNA/Editor/Core/ComponentDescriptor.hpp"
#include "CNA/Editor/Panels/EditorPanel.hpp"

namespace CNA::Editor
{
    class EditorEntity;

    /**
     * @brief Draws every component on the selected entity, and adds or removes them.
     *
     * Generic over component types it was never compiled against: one propertyField() call per
     * declared property is all it takes, which is the payoff of the descriptor system (D-05).
     */
    class InspectorPanel final : public EditorPanel
    {
    public:
        using EditorPanel::EditorPanel;

        void draw() override;

    private:
        /** @brief Draws the "Add Component" picker for @p entity. */
        void drawAddComponentControl(const EditorEntity& entity);

        /**
         * @brief Draws one property row, returning the new value when the user changed it.
         *
         * Quaternions are shown as Euler angles in degrees; everything else goes straight to the
         * matching widget.
         */
        [[nodiscard]] std::optional<PropertyValue> drawPropertyRow(const Uuid& entityId,
                                                                   const std::string& componentTypeId,
                                                                   const PropertyDescriptor& property,
                                                                   const PropertyValue& value);

        /**
         * @brief Takes a dropped asset for @p property, if one landed on the widget just drawn.
         * @return The new reference, or std::nullopt when nothing was dropped or the kind is wrong.
         */
        [[nodiscard]] std::optional<PropertyValue> acceptAssetDrop(const PropertyDescriptor& property);

        /** @brief Payload type for an asset dragged out of the browser. */
        static constexpr const char* kAssetDragType = "asset";

        /**
         * @brief The component type the Add picker is showing.
         *
         * A type id rather than a list index: the list shortens the moment a unique component is
         * added, and an index would then quietly refer to a different type than the one on screen.
         */
        std::string addComponentChoice_;

        /**
         * @brief The Euler angles the user last typed, and the quaternion they produced.
         *
         * A quaternion has more than one Euler spelling, so converting back and forth every frame
         * would let the numbers jump while they are being edited -- type 90 into pitch and the yaw
         * and roll beside it flip to an equivalent pair. Remembering what was typed, and reusing it
         * for as long as the stored quaternion is still the one it produced, keeps the field steady
         * while still following an undo, a gizmo drag or a reload the instant one lands.
         */
        struct EulerEdit
        {
            Uuid entityId;
            std::string componentTypeId;
            std::string propertyName;
            EditorVector3 degrees;
            EditorQuaternion produced;

            [[nodiscard]] bool matches(const Uuid& entity, const std::string& component,
                                       const std::string& property) const
            {
                return entityId == entity && componentTypeId == component && propertyName == property;
            }
        };

        EulerEdit eulerEdit_;
    };
}
