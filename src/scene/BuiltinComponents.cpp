// SPDX-License-Identifier: MS-PL
#include "CNA/Editor/Scene/BuiltinComponents.hpp"

namespace CNA::Editor
{
    namespace
    {
        PropertyDescriptor makeProperty(std::string name,
                                        std::string displayName,
                                        PropertyType type,
                                        PropertyValue defaultValue,
                                        std::string tooltip = {})
        {
            PropertyDescriptor property;
            property.name = std::move(name);
            property.displayName = std::move(displayName);
            property.type = type;
            property.defaultValue = std::move(defaultValue);
            property.tooltip = std::move(tooltip);
            return property;
        }

        ComponentDescriptor makeTransform()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kTransform;
            descriptor.displayName = "Transform";
            descriptor.category = "Core";
            descriptor.unique = true;
            // An entity without a transform has no position, so every viewport operation would
            // need a special case. Making it non-removable is cheaper than making it optional.
            descriptor.required = true;
            descriptor.properties = {
                makeProperty("position", "Position", PropertyType::Vector3, PropertyValue{EditorVector3{}},
                             "Local position relative to the parent entity."),
                makeProperty("rotation", "Rotation", PropertyType::Quaternion, PropertyValue{EditorQuaternion{}},
                             "Local rotation. The inspector presents this as Euler angles; the "
                             "stored value is always a quaternion."),
                makeProperty("scale", "Scale", PropertyType::Vector3,
                             PropertyValue{EditorVector3{1.0f, 1.0f, 1.0f}}, "Local scale."),
            };
            return descriptor;
        }

        ComponentDescriptor makeSpriteRenderer()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kSpriteRenderer;
            descriptor.displayName = "Sprite Renderer";
            descriptor.category = "Rendering";

            PropertyDescriptor layerDepth = makeProperty(
                "layerDepth", "Layer Depth", PropertyType::Float, PropertyValue{0.0f},
                "SpriteBatch layer depth: 0 is front, 1 is back, matching XNA's own convention.");
            layerDepth.minimum = 0.0;
            layerDepth.maximum = 1.0;

            PropertyDescriptor flip = makeProperty("spriteEffects", "Sprite Effects", PropertyType::Enum,
                                                   PropertyValue{PropertyValue::EnumValue{"None"}},
                                                   "Mirrors Microsoft::Xna::Framework::Graphics::SpriteEffects.");
            flip.enumOptions = {"None", "FlipHorizontally", "FlipVertically", "FlipBoth"};

            PropertyDescriptor texture = makeProperty("texture", "Texture", PropertyType::AssetReference,
                                                       PropertyValue{PropertyValue::AssetReference{}},
                                                       "Texture2D asset drawn by this sprite.");
            texture.assetType = "Texture2D";

            descriptor.properties = {
                std::move(texture),
                makeProperty("sourceRectangle", "Source Rectangle", PropertyType::Rectangle,
                             PropertyValue{EditorRectangle{}},
                             "Sub-region of the texture to draw. An empty rectangle means the whole texture."),
                makeProperty("tint", "Tint", PropertyType::Color, PropertyValue{EditorColor{}},
                             "Multiplied into the sampled texel, like SpriteBatch::Draw's color parameter."),
                makeProperty("origin", "Origin", PropertyType::Vector2, PropertyValue{EditorVector2{}},
                             "Rotation and scaling pivot, in texels."),
                std::move(layerDepth),
                std::move(flip),
            };
            return descriptor;
        }

        ComponentDescriptor makeCamera()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kCamera;
            descriptor.displayName = "Camera";
            descriptor.category = "Rendering";

            PropertyDescriptor projection = makeProperty("projection", "Projection", PropertyType::Enum,
                                                         PropertyValue{PropertyValue::EnumValue{"Orthographic"}},
                                                         "Orthographic is the 2D default; Perspective becomes "
                                                         "meaningful in plan.md Phase 3.");
            projection.enumOptions = {"Orthographic", "Perspective"};

            PropertyDescriptor fieldOfView = makeProperty("fieldOfView", "Field Of View", PropertyType::Float,
                                                          PropertyValue{45.0f},
                                                          "Vertical field of view in degrees. Perspective only.");
            fieldOfView.minimum = 1.0;
            fieldOfView.maximum = 179.0;

            descriptor.properties = {
                std::move(projection),
                makeProperty("orthographicSize", "Orthographic Size", PropertyType::Float, PropertyValue{600.0f},
                             "Visible height in world units. Orthographic only."),
                std::move(fieldOfView),
                makeProperty("nearPlane", "Near Plane", PropertyType::Float, PropertyValue{0.1f}),
                makeProperty("farPlane", "Far Plane", PropertyType::Float, PropertyValue{1000.0f}),
                makeProperty("clearColor", "Clear Color", PropertyType::Color,
                             PropertyValue{EditorColor{100, 149, 237, 255}},
                             "Defaults to XNA's CornflowerBlue, because of course it does."),
                makeProperty("isPrimary", "Primary", PropertyType::Boolean, PropertyValue{true},
                             "The camera the game starts with. Exactly one should be primary."),
            };
            return descriptor;
        }

        ComponentDescriptor makeAudioSource()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kAudioSource;
            descriptor.displayName = "Audio Source";
            descriptor.category = "Audio";

            PropertyDescriptor volume = makeProperty("volume", "Volume", PropertyType::Float, PropertyValue{1.0f});
            volume.minimum = 0.0;
            volume.maximum = 1.0;

            PropertyDescriptor pan = makeProperty("pan", "Pan", PropertyType::Float, PropertyValue{0.0f},
                                                  "-1 is fully left, +1 fully right.");
            pan.minimum = -1.0;
            pan.maximum = 1.0;

            PropertyDescriptor pitch = makeProperty("pitch", "Pitch", PropertyType::Float, PropertyValue{0.0f},
                                                    "XNA's pitch range, in octaves.");
            pitch.minimum = -1.0;
            pitch.maximum = 1.0;

            PropertyDescriptor clip = makeProperty("clip", "Clip", PropertyType::AssetReference,
                                                    PropertyValue{PropertyValue::AssetReference{}},
                                                    "SoundEffect asset to play.");
            clip.assetType = "SoundEffect";

            descriptor.properties = {
                std::move(clip),
                makeProperty("playOnAwake", "Play On Awake", PropertyType::Boolean, PropertyValue{false}),
                makeProperty("loop", "Loop", PropertyType::Boolean, PropertyValue{false}),
                std::move(volume),
                std::move(pan),
                std::move(pitch),
                makeProperty("is3D", "3D Positioned", PropertyType::Boolean, PropertyValue{false},
                             "When set, the source is positioned by its Transform via AudioEmitter."),
            };
            descriptor.unique = false;
            return descriptor;
        }

        ComponentDescriptor makeModelRenderer()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kModelRenderer;
            descriptor.displayName = "Model Renderer";
            descriptor.category = "Rendering (3D)";
            PropertyDescriptor model = makeProperty("model", "Model", PropertyType::AssetReference,
                                                     PropertyValue{PropertyValue::AssetReference{}},
                                                     "Model asset to draw.");
            model.assetType = "Model";

            descriptor.properties = {
                std::move(model),
                makeProperty("material", "Material Override", PropertyType::AssetReference,
                             PropertyValue{PropertyValue::AssetReference{}},
                             "Optional single material override. A per-mesh material list arrives with "
                             "the PropertyType::List support planned for plan.md Phase 3."),
                makeProperty("castShadows", "Cast Shadows", PropertyType::Boolean, PropertyValue{true}),
                makeProperty("receiveShadows", "Receive Shadows", PropertyType::Boolean, PropertyValue{true}),
            };
            return descriptor;
        }

        ComponentDescriptor makeLight()
        {
            ComponentDescriptor descriptor;
            descriptor.typeId = BuiltinComponentIds::kLight;
            descriptor.displayName = "Light";
            descriptor.category = "Rendering (3D)";

            PropertyDescriptor kind = makeProperty("kind", "Type", PropertyType::Enum,
                                                   PropertyValue{PropertyValue::EnumValue{"Directional"}});
            kind.enumOptions = {"Directional", "Point", "Spot"};

            descriptor.properties = {
                std::move(kind),
                makeProperty("color", "Color", PropertyType::Color, PropertyValue{EditorColor{}}),
                makeProperty("intensity", "Intensity", PropertyType::Float, PropertyValue{1.0f}),
                makeProperty("range", "Range", PropertyType::Float, PropertyValue{10.0f},
                             "Point and Spot only."),
            };
            return descriptor;
        }
    }

    void registerBuiltinComponents(ComponentRegistry& registry)
    {
        registry.registerComponent(makeTransform());
        registry.registerComponent(makeSpriteRenderer());
        registry.registerComponent(makeCamera());
        registry.registerComponent(makeAudioSource());
        registry.registerComponent(makeModelRenderer());
        registry.registerComponent(makeLight());
    }
}
