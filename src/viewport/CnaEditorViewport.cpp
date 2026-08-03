// SPDX-License-Identifier: MS-PL
/**
 * @file CnaEditorViewport.cpp
 * @brief The CNA-backed scene viewport.
 *
 * A thin adapter: it owns an `EditorCamera2D` (which is CNA-free and lives in cna-editor-scene) and
 * delegates the actual drawing to `CnaSceneRenderer`. Keeping the two apart means the renderer can
 * be reused for asset thumbnails and, later, for the per-backend captures plan.md ED-510 needs,
 * without dragging the viewport's interaction state along with it.
 */

#include "CNA/Editor/Viewport/EditorViewport.hpp"

#include <string>

#include "CNA/Editor/Scene/SceneDocument.hpp"
#include "CNA/Editor/Viewport/CnaSceneRenderer.hpp"
#include "CNA/Editor/Viewport/CnaUiRenderer.hpp"
#include "CNA/GraphicsBackendType.hpp"

namespace CNA::Editor
{
    /**
     * @brief Draws the scene with CNA and hands the result to the UI as a texture.
     *
     * Constructed by the host, which owns the graphics device and the UI renderer the rendered
     * target has to be shared through.
     */
    class CnaEditorViewport final : public EditorViewport
    {
    public:
        CnaEditorViewport(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                          const AssetDatabase& assets,
                          CnaUiRenderer& uiRenderer)
            : uiRenderer_(&uiRenderer)
        {
            renderer_.initialize(device, assets);
        }

        ~CnaEditorViewport() override { renderer_.shutdown(); }

        [[nodiscard]] const char* getBackendName() const override
        {
            // Compile-time: CNA resolves its backend at compile time, so there is exactly one in
            // this binary (ANALYSIS.md finding F-01).
            static const std::string name =
                std::string{"cna-"} + std::string{CNA::getCurrentGraphicsBackendName()};
            return name.c_str();
        }

        UiTextureId render(const SceneDocument& scene,
                           int width,
                           int height,
                           const std::vector<Uuid>& selection,
                           GizmoMode gizmoMode) override
        {
            if (width <= 0 || height <= 0) { return kUiTextureNone; }

            camera_.setViewportSize(EditorVector2{static_cast<float>(width), static_cast<float>(height)});

            const SceneRenderStats stats =
                renderer_.render(scene, camera_, width, height, selection, gizmoMode);
            lastStats_ = ViewportStats{stats.spritesDrawn, stats.spritesSkipped, stats.gridLines,
                                       stats.missingTextures};

            return renderer_.shareWithUi(*uiRenderer_);
        }

        [[nodiscard]] EditorVector2 getSpriteSize(const Uuid& assetId) const override
        {
            return renderer_.getSpriteSize(assetId);
        }

        [[nodiscard]] bool isRenderTextureFlippedVertically() const override
        {
            // Compile-time, from the backend this build was compiled against. Not a runtime probe:
            // CNA fixes its backend at compile time, so this is a constant, and a probe would
            // cost a render target and a read-back to learn something already known.
            switch (CNA::getCurrentGraphicsBackendType())
            {
                case CNA::GraphicsBackendType::EasyGL:
                case CNA::GraphicsBackendType::WebGPU:
                case CNA::GraphicsBackendType::Bgfx:
                case CNA::GraphicsBackendType::SdlGpu:
                    return true;
                default:
                    return false;
            }
        }

        [[nodiscard]] EditorCamera2D& getCamera() override { return camera_; }
        [[nodiscard]] const EditorCamera2D& getCamera() const override { return camera_; }

        [[nodiscard]] ViewportStats getLastStats() const override { return lastStats_; }

    private:
        CnaSceneRenderer renderer_;
        CnaUiRenderer* uiRenderer_;
        EditorCamera2D camera_;
        ViewportStats lastStats_;
    };

    std::unique_ptr<EditorViewport> createCnaEditorViewport(
        Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
        const AssetDatabase& assets,
        CnaUiRenderer& uiRenderer)
    {
        return std::make_unique<CnaEditorViewport>(device, assets, uiRenderer);
    }
}
