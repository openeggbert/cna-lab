// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Viewport/EditorViewport.hpp"

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

namespace CNA::Editor
{
    UiTextureId NullEditorViewport::render(const SceneDocument& scene,
                                           int width,
                                           int height,
                                           const std::vector<Uuid>& selection,
                                           GizmoMode gizmoMode)
    {
        (void)selection;
        (void)gizmoMode;

        width_ = width;
        height_ = height;
        ++renderCount_;

        camera_.setViewportSize(EditorVector2{static_cast<float>(width), static_cast<float>(height)});

        // The geometry is walked even though nothing is drawn. That is the point: a headless run
        // and a real one then exercise the same transform and bounds code, so a crash or an
        // infinite parent chain is caught by CI rather than by the first user to open that scene.
        stats_ = ViewportStats{};
        const SpriteSizeProvider sizeProvider = makeSizeProvider();

        for (const EditorEntity& entity : scene.getEntities())
        {
            if (entity.findComponent(BuiltinComponentIds::kSpriteRenderer) == nullptr) { continue; }

            if (!entity.isEnabled())
            {
                ++stats_.spritesSkipped;
                continue;
            }

            if (computeEntityBounds2D(scene, entity.getId(), sizeProvider))
            {
                ++stats_.spritesDrawn;
                ++stats_.missingTextures;
            }
            else
            {
                ++stats_.spritesSkipped;
            }
        }

        // No texture: the panel renders no image, which is the honest result of drawing nothing.
        return kUiTextureNone;
    }
}
