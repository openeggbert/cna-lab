// SPDX-License-Identifier: MS-PL
/**
 * @file ViewportTests.cpp
 * @brief Tests for world transforms, the editor camera and picking.
 *
 * All of this is CNA-free by design (see SceneTransform.hpp), which is what lets the geometry the
 * viewport, the gizmo and the picker all depend on be verified with no window and no GPU. A bug
 * here would show up as "clicking selects the wrong thing", which is miserable to debug through a
 * running editor and trivial to catch at this level.
 */

#include "TestHarness.hpp"

#include <cmath>

#include "CNA/Editor/Scene/BuiltinComponents.hpp"
#include "CNA/Editor/Scene/EditorCamera2D.hpp"
#include "CNA/Editor/Scene/SceneDocument.hpp"
#include "CNA/Editor/Scene/SceneTransform.hpp"
#include "CNA/Editor/Scene/TranslateGizmo.hpp"

using namespace CNA::Editor;

namespace
{
    ComponentRegistry makeRegistry()
    {
        ComponentRegistry registry;
        registerBuiltinComponents(registry);
        return registry;
    }

    /** @brief Adds an entity with a Transform at (@p x, @p y) and returns its id. */
    Uuid addEntity(SceneDocument& scene, const ComponentRegistry& registry, std::string name,
                   float x, float y, float scale = 1.0f)
    {
        EditorEntity entity{Uuid::generate(), std::move(name)};
        EditorComponent transform{BuiltinComponentIds::kTransform};
        transform.applyDefaults(*registry.find(BuiltinComponentIds::kTransform));
        transform.setProperty("position", PropertyValue{EditorVector3{x, y, 0.0f}});
        transform.setProperty("scale", PropertyValue{EditorVector3{scale, scale, 1.0f}});
        entity.addComponent(std::move(transform));
        return scene.addEntity(std::move(entity));
    }

    /** @brief Gives @p entityId a sprite of the given size, via its source rectangle. */
    void addSprite(SceneDocument& scene, const ComponentRegistry& registry, const Uuid& entityId,
                   int width, int height, float layerDepth = 0.5f)
    {
        EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
        sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
        sprite.setProperty("sourceRectangle", PropertyValue{EditorRectangle{0, 0, width, height}});
        sprite.setProperty("layerDepth", PropertyValue{layerDepth});
        scene.findEntity(entityId)->addComponent(std::move(sprite));
    }

    bool nearlyEqual(float a, float b, float tolerance = 0.001f)
    {
        return std::fabs(a - b) <= tolerance;
    }

    /** @brief A size provider that reports nothing, exercising the unknown-size fallback. */
    const SpriteSizeProvider kNoSizes = [](const Uuid&) { return EditorVector2{}; };
}

CNA_EDITOR_TEST(WorldTransformComposesThroughTheParentChain)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 100.0f, 50.0f, 2.0f);
    const Uuid child = addEntity(scene, registry, "Child", 10.0f, 0.0f);
    scene.reparentEntity(child, parent);

    const std::optional<WorldTransform> world = computeWorldTransform(scene, child);
    CNA_EDITOR_EXPECT(world.has_value());

    // The child's local offset is scaled by the parent before being added: 100 + 10*2.
    CNA_EDITOR_EXPECT(nearlyEqual(world->position.x, 120.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(world->position.y, 50.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(world->scale.x, 2.0f));
}

CNA_EDITOR_TEST(WorldTransformAppliesParentRotation)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 0.0f, 0.0f);
    scene.findEntity(parent)->findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("rotation", PropertyValue{quaternionFromZRotation(3.14159265f * 0.5f)});

    const Uuid child = addEntity(scene, registry, "Child", 10.0f, 0.0f);
    scene.reparentEntity(child, parent);

    const std::optional<WorldTransform> world = computeWorldTransform(scene, child);
    CNA_EDITOR_EXPECT(world.has_value());

    // A quarter turn about Z takes the local +X offset onto world +Y.
    CNA_EDITOR_EXPECT(nearlyEqual(world->position.x, 0.0f, 0.01f));
    CNA_EDITOR_EXPECT(nearlyEqual(world->position.y, 10.0f, 0.01f));
}

CNA_EDITOR_TEST(QuaternionZRotationRoundTrips)
{
    for (float angle : {0.0f, 0.5f, 1.5f, -2.0f, 3.0f})
    {
        CNA_EDITOR_EXPECT(nearlyEqual(zRotationOf(quaternionFromZRotation(angle)), angle, 0.001f));
    }
}

CNA_EDITOR_TEST(EntityBoundsUseTheSourceRectangleAndOrigin)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid id = addEntity(scene, registry, "Sprite", 100.0f, 200.0f);
    addSprite(scene, registry, id, 32, 16);

    const std::optional<WorldBounds2D> bounds = computeEntityBounds2D(scene, id, kNoSizes);
    CNA_EDITOR_EXPECT(bounds.has_value());
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->min.x, 100.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.x, 132.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.y, 216.0f));

    // The origin shifts the sprite, exactly as SpriteBatch::Draw's origin parameter does.
    scene.findEntity(id)->findComponent(BuiltinComponentIds::kSpriteRenderer)
        ->setProperty("origin", PropertyValue{EditorVector2{16.0f, 8.0f}});

    const std::optional<WorldBounds2D> centred = computeEntityBounds2D(scene, id, kNoSizes);
    CNA_EDITOR_EXPECT(nearlyEqual(centred->min.x, 84.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(centred->max.x, 116.0f));
}

CNA_EDITOR_TEST(EntityBoundsFallBackWhenTheTextureSizeIsUnknown)
{
    // A sprite whose texture failed to import must still be clickable, or the entity cannot be
    // selected and therefore cannot be fixed.
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid id = addEntity(scene, registry, "Broken", 0.0f, 0.0f);
    EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
    sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
    sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{Uuid::generate()}});
    scene.findEntity(id)->addComponent(std::move(sprite));

    const std::optional<WorldBounds2D> bounds = computeEntityBounds2D(scene, id, kNoSizes);
    CNA_EDITOR_EXPECT(bounds.has_value());
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.x - bounds->min.x, kUnknownSpriteExtent));
}

CNA_EDITOR_TEST(EntityBoundsUseTheProviderWhenNoSourceRectangleIsSet)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid textureId = Uuid::generate();
    const Uuid id = addEntity(scene, registry, "Sprite", 0.0f, 0.0f);
    EditorComponent sprite{BuiltinComponentIds::kSpriteRenderer};
    sprite.applyDefaults(*registry.find(BuiltinComponentIds::kSpriteRenderer));
    sprite.setProperty("texture", PropertyValue{PropertyValue::AssetReference{textureId}});
    scene.findEntity(id)->addComponent(std::move(sprite));

    const SpriteSizeProvider provider = [textureId](const Uuid& asset) {
        return asset == textureId ? EditorVector2{48.0f, 24.0f} : EditorVector2{};
    };

    const std::optional<WorldBounds2D> bounds = computeEntityBounds2D(scene, id, provider);
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.x, 48.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.y, 24.0f));
}

CNA_EDITOR_TEST(RotatedSpriteBoundsCoverTheRotatedCorners)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid id = addEntity(scene, registry, "Sprite", 0.0f, 0.0f);
    addSprite(scene, registry, id, 100, 10);
    scene.findEntity(id)->findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("rotation", PropertyValue{quaternionFromZRotation(3.14159265f * 0.5f)});

    // Rotated a quarter turn, a 100x10 sprite occupies a 10x100 box. Taking the AABB before the
    // rotation instead would leave most of the sprite unclickable.
    const std::optional<WorldBounds2D> bounds = computeEntityBounds2D(scene, id, kNoSizes);
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.x - bounds->min.x, 10.0f, 0.01f));
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.y - bounds->min.y, 100.0f, 0.01f));
}

CNA_EDITOR_TEST(HierarchyBoundsCoverDescendants)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 0.0f, 0.0f);
    addSprite(scene, registry, parent, 10, 10);

    const Uuid child = addEntity(scene, registry, "Child", 500.0f, 0.0f);
    addSprite(scene, registry, child, 10, 10);
    scene.reparentEntity(child, parent);

    // Framing a parent whose children spread across the level should show the children.
    const std::optional<WorldBounds2D> bounds = computeHierarchyBounds2D(scene, parent, kNoSizes);
    CNA_EDITOR_EXPECT(bounds.has_value());
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->min.x, 0.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(bounds->max.x, 510.0f));
}

CNA_EDITOR_TEST(CameraRoundTripsBetweenWorldAndScreen)
{
    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{100.0f, 50.0f});
    camera.setZoom(2.0f);

    // The camera centre sits at the middle of the viewport by definition.
    const EditorVector2 centreOnScreen = camera.worldToScreen(EditorVector2{100.0f, 50.0f});
    CNA_EDITOR_EXPECT(nearlyEqual(centreOnScreen.x, 400.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(centreOnScreen.y, 300.0f));

    for (const EditorVector2& world : {EditorVector2{0.0f, 0.0f}, EditorVector2{-250.0f, 375.0f},
                                       EditorVector2{1000.0f, -1000.0f}})
    {
        const EditorVector2 back = camera.screenToWorld(camera.worldToScreen(world));
        CNA_EDITOR_EXPECT(nearlyEqual(back.x, world.x, 0.01f));
        CNA_EDITOR_EXPECT(nearlyEqual(back.y, world.y, 0.01f));
    }
}

CNA_EDITOR_TEST(CameraPanTracksTheCursorAtAnyZoom)
{
    for (float zoom : {0.25f, 1.0f, 4.0f})
    {
        EditorCamera2D camera;
        camera.setViewportSize(EditorVector2{800.0f, 600.0f});
        camera.setZoom(zoom);

        const EditorVector2 grabScreen{200.0f, 150.0f};
        const EditorVector2 grabWorld = camera.screenToWorld(grabScreen);

        const EditorVector2 delta{60.0f, -25.0f};
        camera.panByScreenDelta(delta);

        // The world point grabbed must end up under the cursor's new position, or a drag drifts.
        const EditorVector2 nowAt = camera.worldToScreen(grabWorld);
        CNA_EDITOR_EXPECT(nearlyEqual(nowAt.x, grabScreen.x + delta.x, 0.01f));
        CNA_EDITOR_EXPECT(nearlyEqual(nowAt.y, grabScreen.y + delta.y, 0.01f));
    }
}

CNA_EDITOR_TEST(CameraZoomKeepsTheAnchorPointFixed)
{
    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{10.0f, 20.0f});

    const EditorVector2 anchor{620.0f, 110.0f};
    const EditorVector2 anchorWorld = camera.screenToWorld(anchor);

    camera.zoomAt(anchor, 2.5f);

    // Wheel-zoom must keep what is under the pointer under the pointer; zooming about the view
    // centre instead makes the user chase their target across the screen.
    const EditorVector2 after = camera.worldToScreen(anchorWorld);
    CNA_EDITOR_EXPECT(nearlyEqual(after.x, anchor.x, 0.01f));
    CNA_EDITOR_EXPECT(nearlyEqual(after.y, anchor.y, 0.01f));
    CNA_EDITOR_EXPECT(nearlyEqual(camera.getZoom(), 2.5f));
}

CNA_EDITOR_TEST(CameraZoomStaysWithinItsLimits)
{
    EditorCamera2D camera;
    camera.setZoom(1000.0f);
    CNA_EDITOR_EXPECT(nearlyEqual(camera.getZoom(), EditorCamera2D::kMaxZoom));

    camera.setZoom(0.0f);
    CNA_EDITOR_EXPECT(nearlyEqual(camera.getZoom(), EditorCamera2D::kMinZoom));

    // Even when clamping bites, the anchor must not jump -- that is why zoomAt re-derives the
    // anchor's position after setZoom rather than computing the new centre algebraically.
    camera.setZoom(EditorCamera2D::kMaxZoom);
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    const EditorVector2 anchor{100.0f, 100.0f};
    const EditorVector2 anchorWorld = camera.screenToWorld(anchor);
    camera.zoomAt(anchor, 10.0f);
    const EditorVector2 after = camera.worldToScreen(anchorWorld);
    CNA_EDITOR_EXPECT(nearlyEqual(after.x, anchor.x, 0.01f));
}

CNA_EDITOR_TEST(CameraFramesBoundsWithinTheViewport)
{
    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    WorldBounds2D bounds;
    bounds.min = EditorVector2{-100.0f, -50.0f};
    bounds.max = EditorVector2{300.0f, 150.0f};
    camera.frame(bounds, 0.1f);

    CNA_EDITOR_EXPECT(nearlyEqual(camera.getCenter().x, 100.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(camera.getCenter().y, 50.0f));

    // Everything asked for must actually be on screen, with the margin respected.
    const EditorVector2 topLeft = camera.worldToScreen(bounds.min);
    const EditorVector2 bottomRight = camera.worldToScreen(bounds.max);
    CNA_EDITOR_EXPECT(topLeft.x >= 0.0f && topLeft.y >= 0.0f);
    CNA_EDITOR_EXPECT(bottomRight.x <= 800.0f && bottomRight.y <= 600.0f);
}

CNA_EDITOR_TEST(PickingSelectsTheEntityUnderTheCursor)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid left = addEntity(scene, registry, "Left", 0.0f, 0.0f);
    addSprite(scene, registry, left, 50, 50);
    const Uuid right = addEntity(scene, registry, "Right", 200.0f, 0.0f);
    addSprite(scene, registry, right, 50, 50);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{100.0f, 25.0f});

    CNA_EDITOR_EXPECT(pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{25.0f, 25.0f}),
                                   kNoSizes).entityId == left);
    CNA_EDITOR_EXPECT(pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{225.0f, 25.0f}),
                                   kNoSizes).entityId == right);

    // Empty space selects nothing rather than the nearest thing.
    CNA_EDITOR_EXPECT(!pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{120.0f, 25.0f}),
                                    kNoSizes).entityId.isValid());
}

CNA_EDITOR_TEST(PickingPrefersTheFrontmostSprite)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid back = addEntity(scene, registry, "Back", 0.0f, 0.0f);
    addSprite(scene, registry, back, 100, 100, 0.9f);
    const Uuid front = addEntity(scene, registry, "Front", 0.0f, 0.0f);
    addSprite(scene, registry, front, 100, 100, 0.1f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{50.0f, 50.0f});

    // XNA's convention: 0 is front, 1 is back.
    CNA_EDITOR_EXPECT(pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{50.0f, 50.0f}),
                                   kNoSizes).entityId == front);
}

CNA_EDITOR_TEST(PickingIgnoresDisabledEntities)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid hidden = addEntity(scene, registry, "Hidden", 0.0f, 0.0f);
    addSprite(scene, registry, hidden, 100, 100, 0.1f);
    scene.findEntity(hidden)->setEnabled(false);

    const Uuid visible = addEntity(scene, registry, "Visible", 0.0f, 0.0f);
    addSprite(scene, registry, visible, 100, 100, 0.9f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{50.0f, 50.0f});

    // The disabled entity is in front, so picking it would be the natural bug.
    CNA_EDITOR_EXPECT(pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{50.0f, 50.0f}),
                                   kNoSizes).entityId == visible);
}

CNA_EDITOR_TEST(PickingHonoursParentTransforms)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 1000.0f, 500.0f, 2.0f);
    const Uuid child = addEntity(scene, registry, "Child", 10.0f, 0.0f);
    addSprite(scene, registry, child, 20, 20);
    scene.reparentEntity(child, parent);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{1030.0f, 520.0f});

    // The child sits at 1000 + 10*2 = 1020, scaled 2x so 40 units wide.
    CNA_EDITOR_EXPECT(pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{1040.0f, 520.0f}),
                                   kNoSizes).entityId == child);
    CNA_EDITOR_EXPECT(!pickEntityAt(scene, camera, camera.worldToScreen(EditorVector2{1010.0f, 520.0f}),
                                    kNoSizes).entityId.isValid());
}

// ---------------------------------------------------------------------------------------------
// Translate gizmo
// ---------------------------------------------------------------------------------------------

CNA_EDITOR_TEST(GizmoLayoutSitsOnTheEntityAndKeepsItsScreenSize)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;
    const Uuid id = addEntity(scene, registry, "Target", 300.0f, 120.0f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});
    camera.setCenter(EditorVector2{300.0f, 120.0f});

    const std::optional<TranslateGizmoLayout> layout = computeTranslateGizmoLayout(scene, camera, id);
    CNA_EDITOR_EXPECT(layout.has_value());
    CNA_EDITOR_EXPECT(nearlyEqual(layout->origin.x, 400.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(layout->origin.y, 300.0f));

    // Handles are sized in screen pixels, so the gizmo stays grabbable at any zoom. A gizmo that
    // shrinks as you zoom out is one you cannot grab exactly when you most need to.
    camera.setZoom(0.1f);
    const std::optional<TranslateGizmoLayout> zoomedOut = computeTranslateGizmoLayout(scene, camera, id);
    CNA_EDITOR_EXPECT(nearlyEqual(zoomedOut->axisLength, layout->axisLength));
    CNA_EDITOR_EXPECT(nearlyEqual(zoomedOut->centerExtent, layout->centerExtent));
}

CNA_EDITOR_TEST(GizmoHitTestDistinguishesItsHandles)
{
    TranslateGizmoLayout layout;
    layout.origin = EditorVector2{100.0f, 100.0f};

    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{100.0f, 100.0f}) == GizmoHandle::Both);
    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{150.0f, 100.0f}) == GizmoHandle::XAxis);
    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{100.0f, 150.0f}) == GizmoHandle::YAxis);
    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{300.0f, 300.0f}) == GizmoHandle::None);

    // Past the tip is a miss: the arms are segments, not infinite lines.
    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{100.0f + layout.axisLength + 30.0f, 100.0f})
                      == GizmoHandle::None);

    // Slightly off an arm still counts, within the grab tolerance.
    CNA_EDITOR_EXPECT(hitTestTranslateGizmo(layout, EditorVector2{150.0f, 104.0f}) == GizmoHandle::XAxis);
}

CNA_EDITOR_TEST(GizmoDragConstrainsToTheGrabbedAxis)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;
    const Uuid id = addEntity(scene, registry, "Target", 0.0f, 0.0f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    TranslateGizmoDrag drag;
    CNA_EDITOR_EXPECT(drag.begin(scene, camera, id, GizmoHandle::XAxis,
                                 camera.worldToScreen(EditorVector2{0.0f, 0.0f})));
    CNA_EDITOR_EXPECT(drag.isActive());

    // A diagonal cursor move on the X arm must move only in X.
    const std::optional<EditorVector3> moved =
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{50.0f, 80.0f}));
    CNA_EDITOR_EXPECT(moved.has_value());
    CNA_EDITOR_EXPECT(nearlyEqual(moved->x, 50.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(moved->y, 0.0f));

    drag.end();
    CNA_EDITOR_EXPECT(!drag.isActive());
    CNA_EDITOR_EXPECT(!drag.update(scene, camera, EditorVector2{0.0f, 0.0f}).has_value());
}

CNA_EDITOR_TEST(GizmoDragMeasuresFromTheGrabPointNotTheEntityOrigin)
{
    // Grabbing an arm away from its root must not teleport the entity to the cursor: the offset
    // between cursor and entity has to survive the whole drag.
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;
    const Uuid id = addEntity(scene, registry, "Target", 10.0f, 20.0f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    TranslateGizmoDrag drag;
    CNA_EDITOR_EXPECT(drag.begin(scene, camera, id, GizmoHandle::Both,
                                 camera.worldToScreen(EditorVector2{60.0f, 20.0f})));

    const std::optional<EditorVector3> moved =
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{70.0f, 25.0f}));
    CNA_EDITOR_EXPECT(nearlyEqual(moved->x, 20.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(moved->y, 25.0f));
}

CNA_EDITOR_TEST(GizmoDragDoesNotDriftOverManyUpdates)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;
    const Uuid id = addEntity(scene, registry, "Target", 0.0f, 0.0f);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    TranslateGizmoDrag drag;
    drag.begin(scene, camera, id, GizmoHandle::Both, camera.worldToScreen(EditorVector2{0.0f, 0.0f}));

    // Every update is measured from the grab point, so wandering around and returning must land
    // exactly back at the start. An implementation that accumulated frame deltas would not.
    for (int step = 0; step < 200; ++step)
    {
        const float t = static_cast<float>(step);
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{t * 3.0f, -t * 1.5f}));
    }

    const std::optional<EditorVector3> back =
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{0.0f, 0.0f}));
    CNA_EDITOR_EXPECT(nearlyEqual(back->x, 0.0f, 0.001f));
    CNA_EDITOR_EXPECT(nearlyEqual(back->y, 0.0f, 0.001f));
}

CNA_EDITOR_TEST(GizmoDragHonoursAScaledParent)
{
    // The position property is local. Dragging a child of a 2x parent by 100 world units must
    // change its stored position by 50, or the gizmo drifts on every child.
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 0.0f, 0.0f, 2.0f);
    const Uuid child = addEntity(scene, registry, "Child", 0.0f, 0.0f);
    scene.reparentEntity(child, parent);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    TranslateGizmoDrag drag;
    drag.begin(scene, camera, child, GizmoHandle::Both, camera.worldToScreen(EditorVector2{0.0f, 0.0f}));

    const std::optional<EditorVector3> moved =
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{100.0f, 0.0f}));
    CNA_EDITOR_EXPECT(nearlyEqual(moved->x, 50.0f));
}

CNA_EDITOR_TEST(GizmoDragHonoursARotatedParent)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;

    const Uuid parent = addEntity(scene, registry, "Parent", 0.0f, 0.0f);
    scene.findEntity(parent)->findComponent(BuiltinComponentIds::kTransform)
        ->setProperty("rotation", PropertyValue{quaternionFromZRotation(3.14159265f * 0.5f)});

    const Uuid child = addEntity(scene, registry, "Child", 0.0f, 0.0f);
    scene.reparentEntity(child, parent);

    EditorCamera2D camera;
    camera.setViewportSize(EditorVector2{800.0f, 600.0f});

    TranslateGizmoDrag drag;
    drag.begin(scene, camera, child, GizmoHandle::Both, camera.worldToScreen(EditorVector2{0.0f, 0.0f}));

    // Under a parent rotated a quarter turn, moving the child along world +Y is a change along its
    // own local -X... or +X depending on handedness; either way the magnitude is what is checked,
    // and the axis the drag did *not* move along must stay put.
    const std::optional<EditorVector3> moved =
        drag.update(scene, camera, camera.worldToScreen(EditorVector2{0.0f, 30.0f}));
    CNA_EDITOR_EXPECT(nearlyEqual(std::fabs(moved->x), 30.0f, 0.01f));
    CNA_EDITOR_EXPECT(nearlyEqual(moved->y, 0.0f, 0.01f));
}

CNA_EDITOR_TEST(WorldDeltaToLocalIsIdentityForRootEntities)
{
    const ComponentRegistry registry = makeRegistry();
    SceneDocument scene;
    const Uuid id = addEntity(scene, registry, "Root", 0.0f, 0.0f);

    const EditorVector2 delta = worldDeltaToLocal(scene, id, EditorVector2{12.0f, -34.0f});
    CNA_EDITOR_EXPECT(nearlyEqual(delta.x, 12.0f));
    CNA_EDITOR_EXPECT(nearlyEqual(delta.y, -34.0f));
}
