// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Viewport/CnaSceneRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <unordered_map>
#include <unordered_set>

#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Rectangle.hpp"
#include "Microsoft/Xna/Framework/Vector2.hpp"
#include "Microsoft/Xna/Framework/Graphics/BlendState.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/RenderTarget2D.hpp"
#include "Microsoft/Xna/Framework/Graphics/SamplerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteBatch.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteEffects.hpp"
#include "Microsoft/Xna/Framework/Graphics/SpriteSortMode.hpp"
#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/SpriteAnimation.hpp"
#include "CNA/Editor/Scene/Tilemap.hpp"
#include "CNA/Editor/Scene/EditorIcons.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"
#include "CNA/Editor/Scene/TranslateGizmo.hpp"
#include "CNA/Editor/Viewport/CnaUiRenderer.hpp"

namespace Xna = Microsoft::Xna::Framework;
namespace XnaGraphics = Microsoft::Xna::Framework::Graphics;

namespace CNA::Editor
{
    namespace
    {
        /** @brief Background behind the scene, distinct from the editor chrome around it. */
        const Xna::Color kBackground{24, 24, 27, 255};
        const Xna::Color kGridMinor{45, 45, 52, 255};
        const Xna::Color kGridMajor{62, 62, 72, 255};
        const Xna::Color kAxis{92, 74, 74, 255};
        const Xna::Color kSelection{255, 158, 46, 255};

        /** @brief Icon colours. One per kind, so a glance is enough to tell them apart. */
        const Xna::Color kIconFrame{92, 92, 104, 255};
        const Xna::Color kIconCamera{124, 190, 255, 255};
        const Xna::Color kIconLight{250, 218, 110, 255};
        const Xna::Color kIconAudio{150, 220, 168, 255};
        const Xna::Color kIconModel{198, 168, 240, 255};

        /** @brief Returns the mark colour for @p kind. */
        const Xna::Color& iconColor(EditorIconKind kind)
        {
            switch (kind)
            {
                case EditorIconKind::Camera: return kIconCamera;
                case EditorIconKind::Light: return kIconLight;
                case EditorIconKind::AudioSource: return kIconAudio;
                case EditorIconKind::Model: return kIconModel;
                case EditorIconKind::None: break;
            }
            return kIconFrame;
        }

        /** @brief Gizmo arm colours: the X/Y/Z-is-red/green/blue convention every editor shares. */
        const Xna::Color kGizmoX{226, 62, 62, 255};
        const Xna::Color kGizmoY{104, 204, 92, 255};
        const Xna::Color kGizmoCenter{236, 226, 96, 255};

        /** @brief Drawn where a sprite's texture is missing, so the entity stays visible. */
        const Xna::Color kMissingTexture{200, 60, 140, 255};

        /**
         * @brief Returns a grid spacing that keeps lines a sensible distance apart on screen.
         *
         * A fixed world spacing is unusable: zoomed out it becomes a solid block, zoomed in it
         * vanishes. Stepping through 1-2-5 decades is the standard answer and keeps the spacing
         * a round number, which matters because the user reads coordinates off it.
         */
        float chooseGridSpacing(float zoom, float targetPixels)
        {
            if (zoom <= 0.0f) { return 0.0f; }

            const float targetWorld = targetPixels / zoom;
            const float decade = std::pow(10.0f, std::floor(std::log10(std::max(targetWorld, 1e-6f))));
            const float normalised = targetWorld / decade;

            if (normalised < 2.0f) { return decade; }
            if (normalised < 5.0f) { return decade * 2.0f; }
            return decade * 5.0f;
        }

        Xna::Color toXnaColor(const EditorColor& color)
        {
            return Xna::Color(static_cast<int>(color.r), static_cast<int>(color.g),
                              static_cast<int>(color.b), static_cast<int>(color.a));
        }

        XnaGraphics::SpriteEffects toSpriteEffects(const std::string& name)
        {
            if (name == "FlipHorizontally") { return XnaGraphics::SpriteEffects::FlipHorizontally; }
            if (name == "FlipVertically") { return XnaGraphics::SpriteEffects::FlipVertically; }
            if (name == "FlipBoth")
            {
                return static_cast<XnaGraphics::SpriteEffects>(
                    static_cast<int>(XnaGraphics::SpriteEffects::FlipHorizontally)
                    | static_cast<int>(XnaGraphics::SpriteEffects::FlipVertically));
            }
            return XnaGraphics::SpriteEffects::None;
        }
    }

    struct CnaSceneRenderer::Impl
    {
        XnaGraphics::GraphicsDevice* device = nullptr;
        const AssetDatabase* assets = nullptr;
        const ComponentRegistry* components = nullptr;

        std::unique_ptr<XnaGraphics::RenderTarget2D> target;
        std::unique_ptr<XnaGraphics::SpriteBatch> spriteBatch;

        /** @brief A 1x1 white texture: every line, outline and placeholder is a stretched quad. */
        std::unique_ptr<XnaGraphics::Texture2D> pixel;

        std::unordered_map<Uuid, std::unique_ptr<XnaGraphics::Texture2D>> textures;

        /**
         * @brief Assets whose load already failed.
         *
         * Remembered so a broken asset costs one attempt rather than one per frame. An editor that
         * retries a missing file sixty times a second stalls on exactly the project that most needs
         * opening.
         */
        std::unordered_set<Uuid> failedTextures;

        int targetWidth = 0;
        int targetHeight = 0;

        /** @brief Ensures the offscreen target matches the requested size. */
        void ensureTarget(int width, int height)
        {
            if (target != nullptr && targetWidth == width && targetHeight == height) { return; }

            target = std::make_unique<XnaGraphics::RenderTarget2D>(*device, width, height);
            targetWidth = width;
            targetHeight = height;
        }

        /** @brief Returns the texture for @p assetId, loading it once, or nullptr. */
        XnaGraphics::Texture2D* resolveTexture(const Uuid& assetId, SceneRenderStats& stats)
        {
            if (!assetId.isValid() || assets == nullptr) { return nullptr; }

            if (const auto found = textures.find(assetId); found != textures.end())
            {
                return found->second.get();
            }
            if (failedTextures.count(assetId) > 0) { return nullptr; }

            const AssetRecord* record = assets->find(assetId);
            if (record == nullptr)
            {
                failedTextures.insert(assetId);
                return nullptr;
            }

            try
            {
                auto texture = std::make_unique<XnaGraphics::Texture2D>(
                    assets->resolvePath(record->sourcePath), *device);
                XnaGraphics::Texture2D* raw = texture.get();
                textures.emplace(assetId, std::move(texture));
                ++stats.texturesLoaded;
                return raw;
            }
            catch (const std::exception&)
            {
                // A texture that will not load is a project problem, not an editor crash: the
                // sprite falls back to a placeholder so the entity is still visible and selectable.
                failedTextures.insert(assetId);
                return nullptr;
            }
        }

        /** @brief Draws an axis-aligned rectangle in screen space, using the 1x1 white texture. */
        void drawRect(const Xna::Rectangle& rect, const Xna::Color& color)
        {
            spriteBatch->Draw(*pixel, rect, std::nullopt, color, 0.0f,
                              Xna::Vector2{0.0f, 0.0f}, XnaGraphics::SpriteEffects::None, 0.0f);
        }

        /** @brief Draws a one-pixel outline just outside @p rect. */
        void drawOutline(const Xna::Rectangle& rect, const Xna::Color& color, int thickness)
        {
            drawRect(Xna::Rectangle{rect.X - thickness, rect.Y - thickness,
                                    rect.Width + thickness * 2, thickness}, color);
            drawRect(Xna::Rectangle{rect.X - thickness, rect.Y + rect.Height,
                                    rect.Width + thickness * 2, thickness}, color);
            drawRect(Xna::Rectangle{rect.X - thickness, rect.Y, thickness, rect.Height}, color);
            drawRect(Xna::Rectangle{rect.X + rect.Width, rect.Y, thickness, rect.Height}, color);
        }

        /** @brief Draws a diamond centred on (@p cx, @p cy), as rows of the 1x1 white texture. */
        void drawDiamond(int cx, int cy, int radius, const Xna::Color& color)
        {
            for (int row = -radius; row <= radius; ++row)
            {
                const int halfWidth = radius - std::abs(row);
                drawRect(Xna::Rectangle{cx - halfWidth, cy + row, halfWidth * 2 + 1, 1}, color);
            }
        }

        /**
         * @brief Draws a triangle whose vertical edge is at @p x, widening towards @p direction.
         *
         * @param direction +1 for a triangle pointing right, -1 for one pointing left.
         */
        void drawTriangleFrom(int x, int cy, int height, int length, int direction,
                              const Xna::Color& color)
        {
            for (int step = 0; step < length; ++step)
            {
                const int half = std::max(1, height * (length - step) / (2 * length));
                drawRect(Xna::Rectangle{x + step * direction, cy - half, 1, half * 2}, color);
            }
        }

        /**
         * @brief Draws one entity icon: a shared badge frame plus a per-kind mark.
         *
         * The frame is common on purpose -- it makes the icons read as one family of editor
         * artefacts rather than as scene content, which matters because they sit in the same
         * picture as the game's own sprites.
         */
        void drawEditorIcon(const EditorIconPlacement& icon, bool selected)
        {
            const int cx = static_cast<int>(std::round(icon.center.x));
            const int cy = static_cast<int>(std::round(icon.center.y));
            const int extent = static_cast<int>(kEditorIconExtent);
            const Xna::Color& color = iconColor(icon.kind);

            const Xna::Rectangle badge{cx - extent, cy - extent, extent * 2, extent * 2};
            drawRect(badge, Xna::Color(20, 20, 24, 190));
            drawOutline(badge, selected ? kSelection : kIconFrame, selected ? 2 : 1);

            switch (icon.kind)
            {
                case EditorIconKind::Camera:
                    // A body with a lens flaring out of it: the shape of every camera icon since
                    // the first one, and recognisable at thirteen pixels.
                    drawRect(Xna::Rectangle{cx - 8, cy - 5, 9, 10}, color);
                    drawTriangleFrom(cx + 2, cy, 14, 6, +1, color);
                    break;

                case EditorIconKind::Light:
                    drawDiamond(cx, cy, 5, color);
                    // Four rays, so it reads as emitting rather than as a solid object.
                    drawRect(Xna::Rectangle{cx - 1, cy - 9, 2, 3}, color);
                    drawRect(Xna::Rectangle{cx - 1, cy + 7, 2, 3}, color);
                    drawRect(Xna::Rectangle{cx - 9, cy - 1, 3, 2}, color);
                    drawRect(Xna::Rectangle{cx + 7, cy - 1, 3, 2}, color);
                    break;

                case EditorIconKind::AudioSource:
                    drawRect(Xna::Rectangle{cx - 8, cy - 3, 4, 6}, color);
                    drawTriangleFrom(cx - 1, cy, 14, 5, -1, color);
                    drawRect(Xna::Rectangle{cx + 4, cy - 4, 2, 8}, color);
                    drawRect(Xna::Rectangle{cx + 7, cy - 6, 2, 12}, color);
                    break;

                case EditorIconKind::Model:
                    // Two offset squares: the cheapest drawing that reads as a box rather than a
                    // rectangle, and it needs no diagonal.
                    drawOutline(Xna::Rectangle{cx - 7, cy - 3, 10, 10}, color, 1);
                    drawOutline(Xna::Rectangle{cx - 3, cy - 7, 10, 10}, color, 1);
                    break;

                case EditorIconKind::None:
                    break;
            }
        }

        /**
         * @brief Draws the translate gizmo: two arms with arrowheads and a centre square.
         *
         * Every dimension comes from the layout, which is the same object the hit-test uses, so
         * what the user sees and what the user can grab cannot drift apart. The arrowheads are
         * stepped triangles built from rows of the 1x1 white texture -- there is no line or polygon
         * primitive in SpriteBatch, and a stepped triangle at this size is indistinguishable from a
         * smooth one while costing nothing but a handful of quads.
         */
        void drawTranslateGizmo(const TranslateGizmoLayout& layout)
        {
            const int originX = static_cast<int>(std::round(layout.origin.x));
            const int originY = static_cast<int>(std::round(layout.origin.y));
            const int length = static_cast<int>(std::round(layout.axisLength));
            const int extent = static_cast<int>(std::round(layout.centerExtent));

            // The arms start outside the centre square, so the square reads as a separate handle --
            // which it is: it is the one that moves on both axes at once.
            constexpr int kArmThickness = 3;
            constexpr int kHeadLength = 12;
            constexpr int kHeadHalfWidth = 6;

            const int armStart = extent + 2;
            const int armEnd = length - kHeadLength;

            if (armEnd > armStart)
            {
                drawRect(Xna::Rectangle{originX + armStart, originY - kArmThickness / 2,
                                        armEnd - armStart, kArmThickness}, kGizmoX);
                drawRect(Xna::Rectangle{originX - kArmThickness / 2, originY + armStart,
                                        kArmThickness, armEnd - armStart}, kGizmoY);
            }

            // World Y points down (EditorCamera2D's convention, inherited from SpriteBatch), so
            // "down the screen" is +Y here and no sign flip is needed for either head.
            for (int step = 0; step < kHeadLength; ++step)
            {
                const int half = std::max(1, kHeadHalfWidth * (kHeadLength - step) / kHeadLength);

                drawRect(Xna::Rectangle{originX + armEnd + step, originY - half, 1, half * 2}, kGizmoX);
                drawRect(Xna::Rectangle{originX - half, originY + armEnd + step, half * 2, 1}, kGizmoY);
            }

            // Outlined rather than filled: the centre handle sits exactly over the entity's origin,
            // and a filled square that size would hide the very sprite being positioned.
            drawOutline(Xna::Rectangle{originX - extent, originY - extent, extent * 2, extent * 2},
                        kGizmoCenter, 2);
        }

        void drawGrid(const EditorCamera2D& camera, SceneRenderStats& stats)
        {
            // Roughly 90 pixels between minor lines: dense enough to judge distance, sparse enough
            // not to become a texture.
            const float spacing = chooseGridSpacing(camera.getZoom(), 90.0f);
            if (spacing <= 0.0f) { return; }

            const WorldBounds2D visible = camera.getVisibleBounds();
            const float firstX = std::floor(visible.min.x / spacing) * spacing;
            const float firstY = std::floor(visible.min.y / spacing) * spacing;

            // Bounded so that a pathological zoom cannot ask for a million draw calls.
            constexpr int kMaxLines = 400;

            int drawn = 0;
            for (float x = firstX; x <= visible.max.x && drawn < kMaxLines; x += spacing, ++drawn)
            {
                const int screenX = static_cast<int>(std::round(camera.worldToScreen(EditorVector2{x, 0.0f}).x));
                // Every fifth line is emphasised, and the axis itself more so again -- without
                // that the grid reads as texture rather than as a measurable scale.
                const bool isMajor = std::fabs(std::fmod(x / spacing, 5.0f)) < 0.001f;
                const bool isAxis = std::fabs(x) < spacing * 0.001f;
                drawRect(Xna::Rectangle{screenX, 0, 1, targetHeight},
                         isAxis ? kAxis : (isMajor ? kGridMajor : kGridMinor));
                ++stats.gridLines;
            }

            drawn = 0;
            for (float y = firstY; y <= visible.max.y && drawn < kMaxLines; y += spacing, ++drawn)
            {
                const int screenY = static_cast<int>(std::round(camera.worldToScreen(EditorVector2{0.0f, y}).y));
                const bool isMajor = std::fabs(std::fmod(y / spacing, 5.0f)) < 0.001f;
                const bool isAxis = std::fabs(y) < spacing * 0.001f;
                drawRect(Xna::Rectangle{0, screenY, targetWidth, 1},
                         isAxis ? kAxis : (isMajor ? kGridMajor : kGridMinor));
                ++stats.gridLines;
            }
        }
    };

    CnaSceneRenderer::CnaSceneRenderer() : impl_(std::make_unique<Impl>()) {}

    CnaSceneRenderer::~CnaSceneRenderer() { shutdown(); }

    void CnaSceneRenderer::initialize(XnaGraphics::GraphicsDevice& device,
                                      const AssetDatabase& assets,
                                      const ComponentRegistry& components)
    {
        impl_->device = &device;
        impl_->assets = &assets;
        impl_->components = &components;
        impl_->spriteBatch = std::make_unique<XnaGraphics::SpriteBatch>(device);

        impl_->pixel = std::make_unique<XnaGraphics::Texture2D>(device, 1, 1);
        const Xna::Color white(255, 255, 255, 255);
        impl_->pixel->SetData(&white, 1);
    }

    void CnaSceneRenderer::shutdown()
    {
        impl_->textures.clear();
        impl_->failedTextures.clear();
        impl_->pixel.reset();
        impl_->spriteBatch.reset();
        impl_->target.reset();
        impl_->device = nullptr;
        impl_->assets = nullptr;
    }

    void CnaSceneRenderer::invalidateTexture(const Uuid& assetId)
    {
        if (!assetId.isValid())
        {
            impl_->textures.clear();
            impl_->failedTextures.clear();
            return;
        }

        impl_->textures.erase(assetId);

        // Clearing the failure memory matters as much as clearing the texture: a load that failed
        // because the file was missing must get another chance once it comes back, or restoring
        // the file would look like it did nothing.
        impl_->failedTextures.erase(assetId);
    }

    XnaGraphics::Texture2D* CnaSceneRenderer::getOrLoadTexture(const Uuid& assetId)
    {
        if (impl_->device == nullptr) { return nullptr; }

        SceneRenderStats ignored;
        return impl_->resolveTexture(assetId, ignored);
    }

    EditorVector2 CnaSceneRenderer::getSpriteSize(const Uuid& assetId) const
    {
        const auto found = impl_->textures.find(assetId);
        if (found == impl_->textures.end()) { return EditorVector2{}; }
        return EditorVector2{static_cast<float>(found->second->getWidthProperty()),
                             static_cast<float>(found->second->getHeightProperty())};
    }

    SpriteSizeProvider CnaSceneRenderer::makeSizeProvider() const
    {
        return [this](const Uuid& assetId) { return getSpriteSize(assetId); };
    }

    UiTextureId CnaSceneRenderer::shareWithUi(CnaUiRenderer& uiRenderer)
    {
        if (impl_->target == nullptr) { return kUiTextureNone; }
        return uiRenderer.adoptTexture(*impl_->target);
    }

    SceneRenderStats CnaSceneRenderer::render(const SceneDocument& scene,
                                              const EditorCamera2D& camera,
                                              int width,
                                              int height,
                                              const std::vector<Uuid>& selection,
                                              GizmoMode gizmoMode,
                                              const AnimationPreview& preview)
    {
        SceneRenderStats stats;
        lastStats_ = stats;

        if (impl_->device == nullptr || impl_->spriteBatch == nullptr) { return stats; }
        if (width <= 0 || height <= 0) { return stats; }

        impl_->ensureTarget(width, height);

        XnaGraphics::GraphicsDevice& device = *impl_->device;
        device.SetRenderTarget(impl_->target.get());
        device.Clear(kBackground);

        // Pass 1: the grid, beneath everything.
        impl_->spriteBatch->Begin(XnaGraphics::SpriteSortMode::Deferred,
                                  XnaGraphics::BlendState::AlphaBlend);
        impl_->drawGrid(camera, stats);
        impl_->spriteBatch->End();

        // Pass 2: the game's own content. BackToFront honours SpriteRenderer.layerDepth, which is
        // XNA's own convention (0 front, 1 back) and the one the game will see at run time.
        impl_->spriteBatch->Begin(XnaGraphics::SpriteSortMode::BackToFront,
                                  XnaGraphics::BlendState::NonPremultiplied);

        const SpriteSizeProvider sizeProvider = makeSizeProvider();

        for (const EditorEntity& entity : scene.getEntities())
        {
            if (!entity.isEnabled()) { ++stats.spritesSkipped; continue; }

            const EditorComponent* sprite = entity.findComponent(BuiltinComponentIds::kSpriteRenderer);
            if (sprite == nullptr) { continue; }

            const std::optional<WorldTransform> world = computeWorldTransform(scene, entity.getId());
            if (!world) { ++stats.spritesSkipped; continue; }

            // An animation drives the sprite it sits beside: its sheet replaces the texture and its
            // current frame replaces the source rectangle. That is what makes the viewport show the
            // same frame the inspector's preview does -- and which frame *is* current comes in as a
            // parameter, because playback is editor state and must never travel in a scene.
            const EditorComponent* animation =
                entity.findComponent(BuiltinComponentIds::kSpriteAnimation);

            SpriteAnimationClip clip;
            if (animation != nullptr)
            {
                clip = readSpriteAnimationClip(
                    *animation,
                    impl_->components != nullptr
                        ? impl_->components->find(BuiltinComponentIds::kSpriteAnimation)
                        : nullptr);
            }
            const bool animated = animation != nullptr && !clip.isEmpty();

            // Frame zero for every animated entity except the one being previewed. An editor that
            // played every clip at once would be unreadable, and one that showed nothing until a
            // preview started would hide the art.
            const std::size_t framePosition =
                animated && preview.isActive() && preview.entityId == entity.getId()
                    ? std::min(preview.position, clip.frames.size() - 1)
                    : 0;

            const Uuid textureId =
                animated
                    ? animation->getProperty(SpriteAnimationKeys::kSheet).get<PropertyValue::AssetReference>().id
                    : sprite->getProperty("texture").get<PropertyValue::AssetReference>().id;
            XnaGraphics::Texture2D* texture = impl_->resolveTexture(textureId, stats);

            const EditorVector2 screenPosition =
                camera.worldToScreen(EditorVector2{world->position.x, world->position.y});
            const EditorVector2 origin = sprite->getProperty("origin").get<EditorVector2>();
            const EditorColor tint = sprite->getProperty("tint").get<EditorColor>();
            const float layerDepth = sprite->getProperty("layerDepth").get<float>(0.5f);
            const float rotation = zRotationOf(world->rotation);

            if (texture == nullptr)
            {
                // No texture: draw the placeholder at the bounds the picker uses, so what the user
                // clicks and what the user sees are the same rectangle.
                ++stats.missingTextures;
                const std::optional<WorldBounds2D> bounds =
                    computeEntityBounds2D(scene, entity.getId(), sizeProvider);
                if (!bounds) { continue; }

                const EditorVector2 topLeft = camera.worldToScreen(bounds->min);
                const EditorVector2 bottomRight = camera.worldToScreen(bounds->max);
                impl_->drawRect(Xna::Rectangle{static_cast<int>(topLeft.x), static_cast<int>(topLeft.y),
                                               static_cast<int>(bottomRight.x - topLeft.x),
                                               static_cast<int>(bottomRight.y - topLeft.y)},
                                kMissingTexture);
                ++stats.spritesDrawn;
                continue;
            }

            std::optional<Xna::Rectangle> sourceRectangle;
            const EditorRectangle source = animated ? clip.getFrameRectangle(framePosition)
                                                    : sprite->getProperty("sourceRectangle").get<EditorRectangle>();
            if (!source.isEmpty())
            {
                sourceRectangle = Xna::Rectangle{source.x, source.y, source.width, source.height};
            }

            // The camera's zoom folds into the sprite's scale, so one SpriteBatch::Draw covers both
            // the entity's own scale and the view's -- no separate view matrix needed, and the
            // result is exactly what the game would draw at that scale.
            const float scaleX = world->scale.x * camera.getZoom();
            const float scaleY = world->scale.y * camera.getZoom();

            impl_->spriteBatch->Draw(*texture,
                                     Xna::Vector2{screenPosition.x, screenPosition.y},
                                     sourceRectangle,
                                     toXnaColor(tint),
                                     rotation,
                                     Xna::Vector2{origin.x, origin.y},
                                     Xna::Vector2{scaleX, scaleY},
                                     toSpriteEffects(sprite->getProperty("spriteEffects")
                                                         .get<PropertyValue::EnumValue>().name),
                                     layerDepth);
            ++stats.spritesDrawn;
        }

        // Tilemaps share the content pass, so a tile and a sprite at the same layer depth sort
        // against each other exactly as they will at run time.
        for (const EditorEntity& entity : scene.getEntities())
        {
            if (!entity.isEnabled()) { continue; }

            const EditorComponent* tilemap = entity.findComponent(BuiltinComponentIds::kTilemap);
            if (tilemap == nullptr) { continue; }

            const std::optional<WorldTransform> world = computeWorldTransform(scene, entity.getId());
            if (!world) { continue; }

            const ComponentDescriptor* descriptor =
                impl_->components != nullptr ? impl_->components->find(BuiltinComponentIds::kTilemap)
                                             : nullptr;

            const TilemapGrid grid = readTilemapGrid(*tilemap, descriptor);
            if (grid.isEmpty()) { continue; }

            const int tileWidth = static_cast<int>(
                tilemap->getPropertyOrDefault(TilemapKeys::kTileWidth, descriptor).get<std::int64_t>(0));
            const int tileHeight = static_cast<int>(
                tilemap->getPropertyOrDefault(TilemapKeys::kTileHeight, descriptor).get<std::int64_t>(0));
            const int sheetColumns = static_cast<int>(
                tilemap->getPropertyOrDefault(TilemapKeys::kSheetColumns, descriptor).get<std::int64_t>(1));
            if (tileWidth <= 0 || tileHeight <= 0 || sheetColumns <= 0) { continue; }

            const Uuid textureId =
                tilemap->getProperty(TilemapKeys::kTileSet).get<PropertyValue::AssetReference>().id;
            XnaGraphics::Texture2D* sheet = impl_->resolveTexture(textureId, stats);
            if (sheet == nullptr) { continue; }

            // Only the cells the viewport can actually show. A 200x200 map is forty thousand draw
            // calls a frame otherwise, nearly all of them offscreen.
            const TileCoordinate first = worldToTile(*world, tileWidth, tileHeight,
                                                     camera.screenToWorld(EditorVector2{0.0f, 0.0f}));
            const TileCoordinate last =
                worldToTile(*world, tileWidth, tileHeight,
                            camera.screenToWorld(EditorVector2{static_cast<float>(width),
                                                                static_cast<float>(height)}));

            const int minX = std::max(0, std::min(first.x, last.x));
            const int maxX = std::min(grid.columns - 1, std::max(first.x, last.x));
            const int minY = std::max(0, std::min(first.y, last.y));
            const int maxY = std::min(grid.rows - 1, std::max(first.y, last.y));

            const float scaleX = world->scale.x * camera.getZoom();
            const float scaleY = world->scale.y * camera.getZoom();

            for (int y = minY; y <= maxY; ++y)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    const std::int64_t tile = grid.at(x, y);
                    if (tile < 0) { continue; }

                    const int sheetX = static_cast<int>(tile % sheetColumns) * tileWidth;
                    const int sheetY = static_cast<int>(tile / sheetColumns) * tileHeight;

                    const EditorVector2 cell = camera.worldToScreen(EditorVector2{
                        world->position.x + static_cast<float>(x * tileWidth) * world->scale.x,
                        world->position.y + static_cast<float>(y * tileHeight) * world->scale.y});

                    impl_->spriteBatch->Draw(*sheet,
                                             Xna::Vector2{cell.x, cell.y},
                                             Xna::Rectangle{sheetX, sheetY, tileWidth, tileHeight},
                                             toXnaColor(EditorColor{}),
                                             0.0f,
                                             Xna::Vector2{0.0f, 0.0f},
                                             Xna::Vector2{scaleX, scaleY},
                                             XnaGraphics::SpriteEffects::None,
                                             0.9f);
                    ++stats.tilesDrawn;
                }
            }
        }
        impl_->spriteBatch->End();

        // Pass 3: the editor's overlay. Separate from the content pass on purpose -- selection
        // outlines are editor artefacts and must never become part of the scene.
        impl_->spriteBatch->Begin(XnaGraphics::SpriteSortMode::Deferred,
                                  XnaGraphics::BlendState::AlphaBlend);

        // Icons first, so a selection outline or the gizmo lands on top of one rather than under.
        for (const EditorIconPlacement& icon : collectEditorIcons(scene, camera))
        {
            const bool selected =
                std::find(selection.begin(), selection.end(), icon.entityId) != selection.end();

            // The badge outline is the *only* selection feedback an icon entity gets: a camera has
            // no bounds, so the outline pass below finds nothing to draw around it.
            impl_->drawEditorIcon(icon, selected);
            ++stats.iconsDrawn;
        }

        for (const Uuid& selectedId : selection)
        {
            const std::optional<WorldBounds2D> bounds =
                computeEntityBounds2D(scene, selectedId, sizeProvider);
            if (!bounds) { continue; }

            const EditorVector2 topLeft = camera.worldToScreen(bounds->min);
            const EditorVector2 bottomRight = camera.worldToScreen(bounds->max);
            impl_->drawOutline(Xna::Rectangle{static_cast<int>(topLeft.x), static_cast<int>(topLeft.y),
                                              static_cast<int>(bottomRight.x - topLeft.x),
                                              static_cast<int>(bottomRight.y - topLeft.y)},
                               kSelection, 2);
        }

        // The gizmo goes on the *primary* selection only. Drawing one per selected entity would
        // put several overlapping manipulators on screen with no way to tell which one a press
        // would grab. Manipulating a whole multi-selection needs a shared pivot first, and is a
        // separate piece of work (plan.md ED-200's multi-select).
        if (gizmoMode == GizmoMode::Translate && !selection.empty())
        {
            const std::optional<TranslateGizmoLayout> layout =
                computeTranslateGizmoLayout(scene, camera, selection.front());
            if (layout) { impl_->drawTranslateGizmo(*layout); }
        }
        impl_->spriteBatch->End();

        device.SetRenderTarget(nullptr);

        lastStats_ = stats;
        return stats;
    }
}
