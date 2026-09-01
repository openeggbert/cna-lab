#pragma once

#include "IronGang/Core/WorldTypes.hpp"

#include <string>
#include <vector>

namespace IronGang
{
    // plan_14 IG-14-011/012: the collision-only geometry that pairs with a district's render models.
    //
    // The convention, stated once here and in docs/mc3-conventions.md: an MC3 object declares a
    // collision **role**, `scripts/extract_collision.py` turns every blocking role into a
    // world-space box in a sidecar next to the generated model, and the game loads that sidecar
    // **independently of the render model**. A detailed mesh is authored `collision="none"` and
    // paired with simple boxes marked `collision="static"` in the same file; for the box props this
    // game currently has, the render box is already simple enough to be its own proxy.
    //
    // Separate from the `.cnj` on purpose: a physics world reconstructed by walking render meshes
    // is a physics world that changes whenever someone re-tessellates a wall.
    struct CollisionProxy
    {
        // The authoring name, kept so a proxy in the wrong place can be traced to its MC3 object.
        std::string name;
        Aabb bounds;
    };

    inline constexpr int kCollisionProxyFileVersion = 1;

    class CollisionProxySet final
    {
    public:
        // Validation refuses an unsupported version, a missing or non-array `proxies`, a proxy
        // without a three-number centre or half-extents, and a non-positive half-extent -- a
        // zero-thickness collider is one objects tunnel through, which is worse than none because
        // it looks present.
        [[nodiscard]] bool LoadFromFile(const std::string& path, std::string& errorMessage);

        [[nodiscard]] bool IsEmpty() const noexcept { return proxies_.empty(); }
        [[nodiscard]] const std::string& GetId() const noexcept { return id_; }
        [[nodiscard]] const std::vector<CollisionProxy>& GetProxies() const noexcept { return proxies_; }

    private:
        std::string id_;
        std::vector<CollisionProxy> proxies_;
    };
}
