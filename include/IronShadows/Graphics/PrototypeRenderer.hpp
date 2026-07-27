#pragma once

#include "IronShadows/Graphics/PrimitiveMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <memory>
#include <optional>

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
        // warehouseModel/vehicleModels replace the procedural warehouse box and sedan with
        // converted CNJ models (MC3 -> glTF -> CNJ) when supplied, proving the Mesh Craft -> CNA
        // runtime loop for production assets. Every other box keeps using the procedural
        // debug renderer.
        void Initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const PrototypeWorld& world,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel = std::nullopt,
                        std::optional<VehicleModelSet> vehicleModels = std::nullopt);

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
    };
}
