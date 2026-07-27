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

    class PrototypeRenderer final
    {
    public:
        // warehouseModel replaces the procedural "warehouse" box with a converted CNJ model
        // (MC3 -> glTF -> CNJ) when supplied, proving the Mesh Craft -> CNA runtime loop for
        // one production asset. Every other box keeps using the procedural debug renderer.
        void Initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
                        const PrototypeWorld& world,
                        std::optional<Microsoft::Xna::Framework::Graphics::Model> warehouseModel = std::nullopt);
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
    };
}
