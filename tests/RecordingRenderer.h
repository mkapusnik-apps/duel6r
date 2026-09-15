#ifndef DUEL6_TEST_RECORDING_RENDERER_H
#define DUEL6_TEST_RECORDING_RENDERER_H

#include "source/renderer/RendererBase.h"
#include "source/FaceList.h"
#include <vector>
#include <array>

namespace Duel6::Test {
// Records actual presenter draw submissions without screenshots or GPU readback.
class RecordingRenderer final : public RendererBase {
public:
    struct Draw { Material material; BlendFunc blend; Float32 right; };
    std::vector<Draw> draws;
    struct Quad {
        std::array<Vector, 4> vertices, uv, projected;
        Matrix view, projection, model;
        Material material;
        BlendFunc blend;
    };
    std::vector<Quad> quads;
    struct BufferDraw {
        std::vector<Vertex> vertices;
        std::vector<Vector> projected;
        Matrix view;
        Material material;
    };
    std::vector<BufferDraw> buffers;
    void recordQuad(const std::array<Vector, 4> &vertices,
                    const std::array<Vector, 4> &uv, const Material &material) {
        Quad draw{vertices, uv, {}, getViewMatrix(), getProjectionMatrix(),
                  getModelMatrix(), material, blend};
        const auto matrix = draw.projection * draw.view * draw.model;
        for (unsigned i = 0; i < 4; ++i) draw.projected[i] = matrix * vertices[i];
        quads.push_back(draw);
    }
    struct Frame { Vector position; Vector size; Float32 width; Color color; };
    std::vector<Frame> frames;
    unsigned uploads = 0;
    BlendFunc blend = BlendFunc::None;
    Info getInfo() override { return {}; }
    Extensions getExtensions() override { return {}; }
    Texture createTexture(const Image &, TextureFilter, bool) override { return static_cast<Texture>(++uploads); }
    void freeTexture(Texture) override {}
    Image makeScreenshot() override { return {}; }
    void setViewport(Int32, Int32, Int32, Int32) override {}
    void enableWireframe(bool) override {}
    void enableDepthTest(bool) override {}
    void enableDepthWrite(bool) override {}
    void setBlendFunc(BlendFunc value) override { blend = value; }
    void setGlobalTime(Float32) override {}
    void clearBuffers() override {}
    void triangle(const Vector &, const Vector &, const Vector &, const Color &) override {}
    void triangle(const Vector &, const Vector &, const Vector &, const Vector &,
                  const Vector &, const Vector &, const Material &) override {}
    void quad(const Vector &p0, const Vector &p1, const Vector &p2, const Vector &p3, const Color &color) override {
        recordQuad({p0, p1, p2, p3}, {}, Material(Texture{}, color));
    }
    void quad(const Vector &p0, const Vector &p1, const Vector &p2, const Vector &p3, const Vector &t0,
              const Vector &t1, const Vector &t2, const Vector &t3, const Material &material) override {
        draws.push_back({material, blend, p2.x});
        recordQuad({p0, p1, p2, p3}, {t0, t1, t2, t3}, material);
    }
    void point(const Vector &, Float32, const Color &) override {}
    void line(const Vector &, const Vector &, Float32, const Color &) override {}
    void frame(const Vector &position, const Vector &size, Float32 width, const Color &color) override {
        frames.push_back({position, size, width, color});
    }
    std::unique_ptr<RendererBuffer> makeBuffer(const FaceList &faces) override {
        class GeometryBuffer final : public RendererBuffer {
            RecordingRenderer &owner;
            std::vector<Vertex> vertices;
        public:
            GeometryBuffer(RecordingRenderer &owner, const FaceList &faces)
                : owner(owner), vertices(faces.getVertexes()) {}
            void update(const FaceList &faces) override { vertices = faces.getVertexes(); }
            void render(const Material &material) override {
                BufferDraw draw{vertices, {}, owner.getViewMatrix(), material};
                const auto matrix = owner.getProjectionMatrix() * owner.getViewMatrix() * owner.getModelMatrix();
                for (const auto &v : vertices) draw.projected.push_back(matrix * Vector(v.x, v.y, v.z));
                owner.buffers.push_back(std::move(draw));
            }
        };
        return std::make_unique<GeometryBuffer>(*this, faces);
    }
    std::unique_ptr<RendererTarget> makeTarget(ScreenParameters) override { return {}; }
};
}
#endif
