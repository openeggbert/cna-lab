// SPDX-License-Identifier: MS-PL
#pragma once

/**
 * @file CNA/Editor/Scene/SceneModels.hpp
 * @brief What the 3D view should draw solid, decided without a GPU (plan.md ED-402).
 *
 * The same shape as `SceneWireframe.hpp` and for the same reason: *which* models to draw, *where*,
 * and *lit by what* are decisions, and decisions belong in this CNA-free module where CI can check
 * them with no device. The viewport is handed a finished list and does nothing but upload and
 * draw it.
 *
 * That split is what makes ED-402 testable at all. A model pass written straight into the
 * viewport could only be verified by looking at a screenshot, and the things most likely to be
 * wrong -- the world matrix, which mirror is applied, whether an entity is lit from the right
 * side -- are all arithmetic that a test can pin exactly.
 *
 * **The one convention that must not drift**, and the reason this file names it twice: the
 * view-projection here is `EditorCamera3D::getViewProjectionMatrix()`, the *same* matrix the
 * wireframe, the picker and the gizmos already go through, and it already contains the Y mirror
 * that converts XNA's Y-up 3D frame to this editor's Y-down world. A model pass that mirrored
 * again would draw models upside down relative to the grid and the gizmos around them; one that
 * built its own matrix would drift from what a user can click on. Neither is theoretical -- both
 * are the first two ways this task can go wrong.
 */

#include <cstddef>
#include <vector>

#include "CNA/Editor/Core/EditorMatrix.hpp"
#include "CNA/Editor/Core/MeshData.hpp"
#include "CNA/Editor/Core/Uuid.hpp"
#include "CNA/Editor/Scene/EditorCamera3D.hpp"
#include "CNA/Editor/Scene/SceneLighting.hpp"

namespace CNA::Editor
{
    class SceneDocument;

    /** @brief One model to draw: its geometry, where it goes, and what lights it. */
    struct ModelDraw
    {
        Uuid entityId;

        /** @brief The model asset. The viewport keys its GPU buffers on this, never on the entity. */
        Uuid modelId;

        /**
         * @brief The geometry, owned by the mesh cache.
         *
         * Borrowed for the frame this batch is drawn in, exactly as `MeshProvider` promises. Never
         * null in a batch: an entity whose mesh has not been imported is left out rather than
         * carried with a null, because a consumer that has to check would be one that could forget.
         */
        const MeshData* mesh = nullptr;

        /** @brief Model space to world space, from the entity's own world transform. */
        EditorMatrix world;

        /**
         * @brief The lighting resolved at this entity's position.
         *
         * Per entity rather than per scene, because that is what makes a point light behave like
         * one -- see `SceneLighting.hpp`. It is the same answer for every directional-only scene,
         * and computing it per draw costs a few multiplications against a list that is at most as
         * long as the scene's lights.
         */
        EffectLighting lighting;

        /** @brief True when this entity is in the selection, so the viewport can mark it. */
        bool selected = false;
    };

    /** @brief Everything the viewport needs for one solid pass. */
    struct SceneModelBatch
    {
        std::vector<ModelDraw> draws;

        /**
         * @brief The camera's own view-projection -- mirror included, applied once.
         *
         * Carried in the batch rather than recomputed by the viewport so that there is exactly one
         * answer to "where does this go on screen", shared by the solid pass, the wireframe drawn
         * over it and the picking that decides what a click hit.
         */
        EditorMatrix viewProjection;

        /**
         * @brief The same camera, split -- because some effects need the view on its own.
         *
         * `viewProjection` stays authoritative and is what everything positions against; these two
         * exist because CNA's `PbrEffect` *inverts the view matrix* to recover the eye position for
         * its specular term, and an identity view would put the eye at the origin and light every
         * model from the wrong place. `projection` carries the Y mirror, exactly as
         * `EditorCamera3D` puts it there, so multiplying these two reproduces `viewProjection`
         * rather than approximating it. `TheModelBatchsSplitCameraMultipliesBackToItsProduct` pins
         * that, because two descriptions of one camera that could drift is precisely the bug this
         * whole seam was arranged to avoid.
         */
        EditorMatrix view;
        EditorMatrix projection;

        /** @brief Entities that name a model whose geometry is not available yet. */
        std::size_t pendingMeshes = 0;

        /** @brief Total triangles across every draw, before any culling the device does. */
        std::size_t triangleCount = 0;
    };

    /**
     * @brief Returns everything in @p scene that a solid model pass should draw.
     *
     * @param meshProvider Where geometry comes from; an entity is left out when this returns
     *        nullptr, and counted in `pendingMeshes` so the viewport can say how many models are
     *        still importing rather than leaving a user wondering where their crate went.
     * @param selection Entities to mark as selected. Order is not significant.
     *
     * Disabled entities are skipped, matching every other pass. An entity with a `ModelRenderer`
     * but no transform is skipped too: there is nowhere to put it, and the origin is not a guess
     * this function is entitled to make.
     */
    [[nodiscard]] SceneModelBatch buildSceneModelBatch(const SceneDocument& scene,
                                                       const EditorCamera3D& camera,
                                                       const MeshProvider& meshProvider,
                                                       const std::vector<Uuid>& selection = {});
}
