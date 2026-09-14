#include <memory>
#include <string>

#include <SDL2/SDL_ttf.h>

#include "tests/TestHarness.h"

#define private public
#include "source/Font.h"
#undef private
#include "source/network/NetworkTrustPolicy.h"

namespace {
    using namespace Duel6;

    class TestRenderer final : public Renderer {
    public:
        unsigned uploads = 0;
        Size uploadedWidth = 0;
        Size uploadedHeight = 0;

        Info getInfo() override { return {}; }
        Extensions getExtensions() override { return {}; }
        Texture createTexture(const Image &image, TextureFilter, bool) override {
            ++uploads;
            uploadedWidth = image.getWidth();
            uploadedHeight = image.getHeight();
            return static_cast<Texture>(uploads);
        }
        void freeTexture(Texture) override {}
        Image makeScreenshot() override { return {}; }
        void setViewport(Int32, Int32, Int32, Int32) override {}
        void setProjectionMatrix(const Matrix &) override {}
        Matrix getProjectionMatrix() const override { return {}; }
        void setViewMatrix(const Matrix &) override {}
        Matrix getViewMatrix() const override { return {}; }
        void setModelMatrix(const Matrix &) override {}
        Matrix getModelMatrix() const override { return {}; }
        void enableWireframe(bool) override {}
        void enableDepthTest(bool) override {}
        void enableDepthWrite(bool) override {}
        void setBlendFunc(BlendFunc) override {}
        void setGlobalTime(Float32) override {}
        void clearBuffers() override {}
        void triangle(const Vector &, const Vector &, const Vector &, const Color &) override {}
        void triangle(const Vector &, const Vector &, const Vector &, const Vector &,
                      const Vector &, const Vector &, const Material &) override {}
        void quad(const Vector &, const Vector &, const Vector &, const Vector &, const Color &) override {}
        void quad(const Vector &, const Vector &, const Vector &, const Vector &, const Vector &,
                  const Vector &, const Vector &, const Vector &, const Material &) override {}
        void quadXY(const Vector &, const Vector &, const Color &) override {}
        void quadXY(const Vector &, const Vector &, const Vector &, const Vector &, const Material &) override {}
        void quadXZ(const Vector &, const Vector &, const Color &) override {}
        void quadXZ(const Vector &, const Vector &, const Vector &, const Vector &, const Material &) override {}
        void quadYZ(const Vector &, const Vector &, const Color &) override {}
        void quadYZ(const Vector &, const Vector &, const Vector &, const Vector &, const Material &) override {}
        void point(const Vector &, Float32, const Color &) override {}
        void line(const Vector &, const Vector &, Float32, const Color &) override {}
        void frame(const Vector &, const Vector &, Float32, const Color &) override {}
        std::unique_ptr<RendererBuffer> makeBuffer(const FaceList &) override { return {}; }
        std::unique_ptr<RendererTarget> makeTarget(ScreenParameters) override { return {}; }
    };
}

D6R_TEST_CASE("NET-AC-017 Font measures code points and renders UTF-8 punctuation and accepted names") {
    D6R_REQUIRE(TTF_Init() == 0);
    {
        TestRenderer renderer;
        Font font(renderer);
        font.font = TTF_OpenFont(D6R_TEST_FONT_PATH, 32);
        D6R_REQUIRE(font.font != nullptr);

        D6R_REQUIRE_EQ(8, font.getTextWidth("A", 16));
        D6R_REQUIRE_EQ(8, font.getTextWidth("•", 16));
        D6R_REQUIRE_EQ(24, font.getTextWidth("Zoë", 16));
        D6R_REQUIRE_EQ(40, font.getTextWidth("– • …", 16));

        const std::string text = "Zoë • Håkon – Reconnecting…";
        D6R_REQUIRE(Network::Trust::validParticipantName("Zoë"));
        D6R_REQUIRE(Network::Trust::validParticipantName("Håkon"));
        const Texture texture = font.renderText(text);
        D6R_REQUIRE(texture != Texture{});
        D6R_REQUIRE_EQ(1u, renderer.uploads);
        D6R_REQUIRE(renderer.uploadedWidth > 0);
        D6R_REQUIRE(renderer.uploadedHeight > 0);
    }
    TTF_Quit();
}
