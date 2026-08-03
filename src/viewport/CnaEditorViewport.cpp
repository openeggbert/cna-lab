// SPDX-License-Identifier: MS-PL
/**
 * @file CnaEditorViewport.cpp
 * @brief The CNA-backed viewport -- the one translation unit in the editor that links CNA.
 *
 * Built only when CNA_EDITOR_WITH_CNA is ON. Everything here goes through CNA's *public* API --
 * `Microsoft::Xna::Framework::Graphics::GraphicsDevice`, `SpriteBatch`, `Texture2D` -- and never
 * through `CNA::Internal::*`. That restraint is the point: if the editor cannot draw a scene using
 * only the API a game has, then neither can a game, and the gap is a CNA bug worth finding
 * (ANALYSIS.md decision D-01).
 *
 * The implementation is a Phase 0 skeleton: it establishes the seam, the include boundary and the
 * build wiring. The real drawing arrives with plan.md ED-120 and ED-201.
 */

#include "CNA/Editor/Viewport/EditorViewport.hpp"

#include <string>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"

#if defined(CNA_EDITOR_HAS_CNA)
#    include "CNA/GraphicsBackendType.hpp"
#endif

namespace CNA::Editor
{
    namespace
    {
        /**
         * @brief Draws a scene through CNA's public graphics API.
         *
         * The GraphicsDevice is *borrowed*, never created here. The editor application owns the
         * window and the device; the viewport only draws into whatever it is handed, which is what
         * lets the same class serve both the docked editor viewport and an offscreen render used
         * for asset thumbnails.
         */
        class CnaEditorViewport final : public EditorViewport
        {
        public:
            [[nodiscard]] const char* getBackendName() const override
            {
#if defined(CNA_EDITOR_HAS_CNA)
                // CNA resolves this at compile time -- there is exactly one backend in this
                // binary, and it cannot be changed at run time. See ANALYSIS.md finding F-01.
                static const std::string name =
                    std::string{"cna-"} + std::string{CNA::getCurrentGraphicsBackendName()};
                return name.c_str();
#else
                return "cna-unavailable";
#endif
            }

            void resize(int width, int height) override
            {
                width_ = width;
                height_ = height;
            }

            void renderScene(const SceneDocument& scene) override
            {
                // plan.md ED-201: walk the entities, resolve each CNA.SpriteRenderer's texture
                // through the AssetDatabase, and issue one SpriteBatch::Draw per sprite ordered by
                // layer depth.
                (void)scene;
            }

            void renderGrid() override
            {
                // plan.md ED-202.
            }

            void renderSelectionOutline(const std::vector<Uuid>& selection) override
            {
                // plan.md ED-203. Drawn as an overlay pass, never as scene geometry.
                (void)selection;
            }

            void renderIcons(const SceneDocument& scene) override
            {
                // plan.md ED-204: billboarded icons for cameras, lights and audio sources, which
                // have no geometry of their own and would otherwise be unclickable.
                (void)scene;
            }

            void renderGizmos(const std::vector<Uuid>& selection, GizmoMode mode) override
            {
                // plan.md ED-205 (translate), ED-401 (rotate and scale).
                (void)selection;
                (void)mode;
            }

            [[nodiscard]] PickResult pick(const SceneDocument& scene, int x, int y) const override
            {
                // plan.md ED-206: ray-cast against each entity's editor-side bounds. GPU picking
                // through an id target is deferred to ED-320, and only if profiling asks for it.
                (void)scene;
                (void)x;
                (void)y;
                return PickResult{};
            }

            [[nodiscard]] ViewportCamera& getCamera() override { return camera_; }
            [[nodiscard]] ViewportMode getMode() const override { return mode_; }
            void setMode(ViewportMode mode) override { mode_ = mode; }

        private:
            ViewportCamera camera_;
            ViewportMode mode_ = ViewportMode::TwoDimensional;
            int width_ = 0;
            int height_ = 0;
        };
    }

    /** @brief Creates the CNA-backed viewport. Declared in the module's own factory header. */
    std::unique_ptr<EditorViewport> createCnaEditorViewport()
    {
        return std::make_unique<CnaEditorViewport>();
    }
}
