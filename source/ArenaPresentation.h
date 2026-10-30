#ifndef DUEL6_ARENAPRESENTATION_H
#define DUEL6_ARENAPRESENTATION_H

#include <algorithm>
#include <cmath>
#include "FaceList.h"
#include "math/Math.h"
#include "renderer/Renderer.h"

namespace Duel6 {
    // Immutable, non-owning geometry view. Neither adapter gives the renderer
    // access to mutable gameplay, transport, persistence or input state.
    struct ArenaGeometryScene {
        const FaceList &walls;
        const FaceList &decorations;
        const FaceList &water;
        Texture blocks;
        Float32 worldSeconds;
    };

    namespace ArenaPresentation {
        inline Vector cameraPosition(Int32 levelWidth, Int32 levelHeight,
                                     Float32 aspect, Float32 fieldOfView) {
            const Float32 tangent = Math::degTan(fieldOfView / 2.0f);
            const Float32 distance = std::max(levelHeight / (2.0f * tangent),
                                              levelWidth / (2.0f * aspect * tangent));
            return Vector(levelWidth / 2.0f, levelHeight / 2.0f, distance + 1.0f);
        }

        inline void staticGeometry(Renderer &renderer, const ArenaGeometryScene &scene) {
            renderer.setGlobalTime(scene.worldSeconds);
            renderer.enableDepthTest(true);
            renderer.enableDepthWrite(true);
            renderer.setBlendFunc(BlendFunc::None);
            scene.walls.render(scene.blocks, false);
            scene.decorations.render(scene.blocks, true);
        }

        inline void water(Renderer &renderer, const ArenaGeometryScene &scene) {
            renderer.setGlobalTime(scene.worldSeconds);
            renderer.enableDepthTest(true);
            renderer.enableDepthWrite(false);
            renderer.setBlendFunc(BlendFunc::SrcColor);
            scene.water.render(scene.blocks, false);
            renderer.setBlendFunc(BlendFunc::None);
            renderer.enableDepthWrite(true);
        }

        // Elevator positions are the left edge of the supporting top surface,
        // not a sprite centre. Share all five faces, including their winding.
        inline void elevator(Renderer &renderer, Texture texture, const Vector &position) {
            const Float32 x = position.x, y = position.y - 0.3f;
            const Material material = Material::makeTexture(texture);
            renderer.quadXY(Vector(x, y, 0.7f), Vector(1.0f, 0.3f), Vector::ZERO, Vector(1, 1), material);
#ifdef D6_RENDER_BACKS
            renderer.quadXY(Vector(x + 1, y, 0.7f), Vector(-1.0f, 0.3f), Vector::ZERO, Vector(1, 1), material);
#endif
            renderer.quadYZ(Vector(x, y, 0.3f), Vector(0.0f, 0.3f, 0.4f), Vector::ZERO, Vector(1, 1), material);
            renderer.quadYZ(Vector(x + 1, y, 0.7f), Vector(0.0f, 0.3f, -0.4f), Vector::ZERO, Vector(1, 1), material);
            renderer.quadXZ(Vector(x, y + 0.3f, 0.3f), Vector(1.0f, 0.0f, 0.4f), Vector::ZERO, Vector(1, 1), material);
            renderer.quadXZ(Vector(x, y, 0.7f), Vector(1.0f, 0.0f, -0.4f), Vector::ZERO, Vector(1, 1), material);
        }

        inline Float32 ratio(Float32 value, Float32 total) {
            return total > 0 && std::isfinite(value) && std::isfinite(total)
                   ? std::clamp(value / total, 0.0f, 1.0f) : 0.0f;
        }
    }
}
#endif
