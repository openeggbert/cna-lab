#pragma once

#include "IronShadows/Graphics/PrimitiveMesh.hpp"

#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"

#include <memory>

namespace IronShadows
{
    class PrototypeWorld;

    class PrototypeRenderer final
    {
    public:
        void Initialize(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device,
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
    };
}
