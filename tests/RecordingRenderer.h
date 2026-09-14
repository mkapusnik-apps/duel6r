#ifndef DUEL6_TEST_RECORDING_RENDERER_H
#define DUEL6_TEST_RECORDING_RENDERER_H

#include "source/renderer/RendererBase.h"
#include <vector>

namespace Duel6::Test {
// Records actual presenter draw submissions without screenshots or GPU readback.
class RecordingRenderer final : public RendererBase {
public:
    struct Draw { Material material; BlendFunc blend; Float32 right; };
    std::vector<Draw> draws;
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
    void quad(const Vector &, const Vector &, const Vector &, const Vector &, const Color &) override {}
    void quad(const Vector &, const Vector &, const Vector &p2, const Vector &, const Vector &,
              const Vector &, const Vector &, const Vector &, const Material &material) override {
        draws.push_back({material, blend, p2.x});
    }
    void point(const Vector &, Float32, const Color &) override {}
    void line(const Vector &, const Vector &, Float32, const Color &) override {}
    std::unique_ptr<RendererBuffer> makeBuffer(const FaceList &) override { return {}; }
    std::unique_ptr<RendererTarget> makeTarget(ScreenParameters) override { return {}; }
};
}
#endif
