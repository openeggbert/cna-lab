// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Robert Vokac and contributors
#pragma once

#ifdef MESH_WORLD_HAS_RENDERER

#include <MeshCraft/Mc3/Mc3Document.hpp>
#include <Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp>
#include <memory>
#include <string>

namespace MeshCraft::Renderer { class SceneRenderer; }

namespace MeshWorld {

// First-person camera state (yaw=horizontal, pitch=vertical, both in radians).
struct FPCamera {
    float x{0.f}, y{2.f}, z{0.f};
    float yaw{0.f};
    float pitch{0.f};
    float fov_y{1.0472f};   // 60 degrees
    float near_z{0.1f};
    float far_z{1000.f};
    float aspect{16.f / 9.f};
};

// Thin wrapper around MeshCraft::Renderer::SceneRenderer that owns a
// SceneRenderer instance and handles view/projection matrix construction
// from a first-person camera for MeshWorld chunk rendering.
class Mc3Renderer {
public:
    explicit Mc3Renderer(Microsoft::Xna::Framework::Graphics::GraphicsDevice& device);
    ~Mc3Renderer();

    // Render a single MC3 chunk document from the current camera position.
    void render(const MeshCraft::Mc3::Mc3Document& doc, const FPCamera& cam);

    // Expose the inner SceneRenderer for advanced use (e.g. WorldRenderer).
    MeshCraft::Renderer::SceneRenderer& scene_renderer() { return *renderer_; }

private:
    std::unique_ptr<MeshCraft::Renderer::SceneRenderer> renderer_;
};

} // namespace MeshWorld

#endif // MESH_WORLD_HAS_RENDERER
