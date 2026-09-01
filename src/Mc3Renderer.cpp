// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors

#ifdef MESH_WORLD_HAS_RENDERER

#include "Mc3Renderer.hpp"
#include <MeshCraft/Renderer/SceneRenderer.hpp>
#include <Microsoft/Xna/Framework/Matrix.hpp>
#include <Microsoft/Xna/Framework/MathHelper.hpp>
#include <Microsoft/Xna/Framework/Vector3.hpp>
#include <cmath>

namespace MeshWorld {

using namespace Microsoft::Xna::Framework;
using namespace MeshCraft::Renderer;

Mc3Renderer::Mc3Renderer(Graphics::GraphicsDevice& device)
    : renderer_(std::make_unique<SceneRenderer>(device))
{}

Mc3Renderer::~Mc3Renderer() = default;

void Mc3Renderer::render(const MeshCraft::Mc3::Mc3Document& doc, const FPCamera& cam) {
    // Build look-at target from yaw/pitch (Y-up, right-handed).
    const float cp = std::cos(cam.pitch);
    const Vector3 forward(
        cp * std::sin(cam.yaw),
        std::sin(cam.pitch),
        cp * -std::cos(cam.yaw)
    );
    const Vector3 pos(cam.x, cam.y, cam.z);
    const Vector3 target = pos + forward;
    const Vector3 up(0.f, 1.f, 0.f);

    const Matrix view = Matrix::CreateLookAt(pos, target, up);
    const Matrix proj = Matrix::CreatePerspectiveFieldOfView(
        cam.fov_y, cam.aspect, cam.near_z, cam.far_z);

    renderer_->draw(doc, view, proj, {});
}

} // namespace MeshWorld

#endif // MESH_WORLD_HAS_RENDERER
