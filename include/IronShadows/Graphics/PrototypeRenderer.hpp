#pragma once

#include "IronShadows/Graphics/PrimitiveMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <memory>
#include <optional>
#include <string>

namespace CNA::Extended::ECS
{
    class World;
}

namespace CNA::Extended::World3DEXT
{
    struct ModelAnimationComponentEXT;
}

namespace IronShadows
{
    class PrototypeWorld;

    // The current MC3 -> glTF -> CNJ pipeline does not bake per-object node transforms into
    // vertex data, so a multi-object MC3 scene loaded as one CNJ Model loses each object's
    // relative position (confirmed empirically; see plan/plan_10-gltf-cnj-mcb-and-runtime-packages.md).
    // Until a scene compiler closes that gap, a multi-part prop like the sedan is authored as one
    // single-object MC3 file per part, and Iron Shadows itself composes the parts with its own
    // local-offset transforms -- the same composition PrototypeRenderer already did for the
    // procedural boxes, just per CNJ model instead of per procedural box.
    struct VehicleModelSet
    {
        Microsoft::Xna::Framework::Graphics::Model body;
        Microsoft::Xna::Framework::Graphics::Model cabin;
        Microsoft::Xna::Framework::Graphics::Model windshield;
        Microsoft::Xna::Framework::Graphics::Model wheel;
    };

    class PrototypeRenderer final
    {
    public:
        PrototypeRenderer();
        // Declared (not defaulted) here and defined `= default` in the .cpp: characterAnimComponent_
        // is a unique_ptr to a forward-declared type, which needs the type complete wherever the
        // destructor is actually instantiated -- the .cpp includes the full definition, this header's
        // other includers do not.
        ~PrototypeRenderer();

        // warehouseModel/vehicleModels replace the procedural warehouse box and sedan with
        // converted CNJ models (MC3 -> glTF -> CNJ) when supplied, proving the Mesh Craft -> CNA
        // runtime loop for production assets. Every other box keeps using the procedural
        // debug renderer.
        // characterModel additionally replaces the procedural on-foot player box with a real
        // skinned CNJ character (gate M6) when supplied; see UpdateCharacterAnimation for how its
        // Idle/Walk clip is driven. characterModel's asset is hand-authored glTF, not MC3 -- Mesh
        // Craft has no rigging/skinning authoring support (see assets/source/gltf/test_character.gltf's
        // own provenance note) -- unlike warehouseModel/vehicleModels, which are real MC3 content.
        void Initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const PrototypeWorld& world,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel = std::nullopt,
                        std::optional<VehicleModelSet> vehicleModels = std::nullopt,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> characterModel = std::nullopt);

        // Advances the skinned character's animation state (gate M6): switches to clipName if it
        // differs from the currently playing clip (a hard cut -- see
        // CNA::Extended::World3DEXT::ModelAnimationSystem3DEXT's own header comment for why there
        // is no blending yet) and ticks it by deltaSeconds. A no-op if characterModel was not
        // supplied to Initialize() (the procedural player box stays in place instead). Call once
        // per frame from gameplay Update(), not Draw() -- Draw() has no time step of its own.
        void UpdateCharacterAnimation(float deltaSeconds, const std::string& clipName);

        // Rebuilds just the static city mesh for a newly loaded district (a district transition,
        // see DistrictManager) without touching the vehicle/player meshes or reloading CNJ
        // content. warehouseModel_/vehicleModels_ keep applying by box name, so re-entering the
        // district that has the warehouse CNJ model still shows it; a district with no box named
        // "warehouse" (e.g. the countryside) naturally never matches and stays fully procedural.
        void RebuildStaticGeometry(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                                   const PrototypeWorld& world);

        void Draw(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                  const Microsoft::Xna::Framework::Matrix& view,
                  const Microsoft::Xna::Framework::Matrix& projection,
                  const Microsoft::Xna::Framework::Vector3& playerPosition,
                  float playerYaw,
                  bool drawPlayer,
                  const Microsoft::Xna::Framework::Vector3& vehiclePosition,
                  float vehicleYaw);

    private:
        void DrawMesh(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                      PrimitiveMesh& mesh,
                      const Microsoft::Xna::Framework::Matrix& worldMatrix);

        PrimitiveMesh staticCityMesh_;
        PrimitiveMesh vehicleMesh_;
        PrimitiveMesh playerMesh_;
        std::unique_ptr<Microsoft::Xna::Framework::Graphics::BasicEffect> effect_;

        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel_;
        Microsoft::Xna::Framework::Vector3 warehousePosition_{};

        std::optional<VehicleModelSet> vehicleModels_;

        // Gate M6: a minimal, dedicated ECS world with exactly one entity, existing only to
        // drive CNA::Extended::World3DEXT::ModelAnimationSystem3DEXT for characterModel_'s
        // ModelAnimationComponentEXT -- rendering still goes through the same direct
        // Model::Draw() call warehouseModel_/vehicleModels_ already use (see Draw()'s .cpp
        // comment for why RenderSystem3DEXT/Camera3DEXT are not introduced just for this).
        std::optional<Microsoft::Xna::Framework::Graphics::Model> characterModel_;
        std::unique_ptr<CNA::Extended::ECS::World> characterWorld_;
        std::unique_ptr<CNA::Extended::World3DEXT::ModelAnimationComponentEXT> characterAnimComponent_;
    };
}
