#include "TestHarness.h"
#include "source/renderer/Renderer.h"
#if defined(D6_RENDERER_GL1)
#include "source/renderer/gl1/GL1Renderer.h"
using TestRenderer = Duel6::GL1Renderer;
#else
#include "source/renderer/gl4/GL4Renderer.h"
using TestRenderer = Duel6::GL4Renderer;
#endif

#include <limits>

namespace {
    using namespace Duel6;

    struct GraphicsContext {
        SDL_Window *window = nullptr;
        SDL_GLContext context = nullptr;

        GraphicsContext() {
            D6R_REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);
#if defined(D6_RENDERER_GL1)
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#else
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
            window = SDL_CreateWindow("Renderer batch regression", 0, 0, 320, 240,
                                      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
            D6R_REQUIRE(window != nullptr);
            context = SDL_GL_CreateContext(window);
            D6R_REQUIRE(context != nullptr);
            D6R_REQUIRE(glewInit() == GLEW_OK);
            // GLEW can probe unsupported enums in a core context.
            while (glGetError() != GL_NO_ERROR) {}
            SDL_GL_SetSwapInterval(0);
        }

        ~GraphicsContext() {
            SDL_GL_DeleteContext(context);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
    };

    void effects(Renderer &renderer, bool batch) {
        for (Int32 player = 0; player < 15; ++player) {
            const Vector centre(30.0f + (player % 5) * 60.0f, 35.0f + (player / 5) * 75.0f, 0.5f);
            Vector points[24];
            Vector segments[72];
            for (Int32 i = 0; i < 24; ++i) {
                points[i] = centre + 14.0f * Vector::direction(i * 15 + player);
            }
            Vector previous = centre + 23.0f * 0.95f * Vector::direction(0);
            for (Int32 i = 1; i <= 36; ++i) {
                const auto position = centre + 23.0f * (i % 2 ? 1.05f : 0.95f) * Vector::direction(i * 10);
                segments[2 * (i - 1)] = previous;
                segments[2 * (i - 1) + 1] = position;
                previous = position;
            }
            if (batch) {
                renderer.points(points, 24, 2.0f, Color::RED);
                renderer.lines(segments, 72, 3.0f, Color::YELLOW);
            } else {
                renderer.Renderer::points(points, 24, 2.0f, Color::RED);
                renderer.Renderer::lines(segments, 72, 3.0f, Color::YELLOW);
            }
        }
    }

    Image scene(Renderer &renderer, Texture texture, bool batch, bool blend) {
        renderer.enableDepthWrite(true);
        renderer.clearBuffers();
        renderer.enableDepthTest(!blend);
        renderer.setBlendFunc(blend ? BlendFunc::SrcAlpha : BlendFunc::None);
        renderer.setViewMatrix(blend ? Matrix::translate(-12, -8, 0) : Matrix::IDENTITY);
        effects(renderer, batch);

        // Overlapping translucent primitives must retain submission order.
        const Vector overlap[] = {{150, 110, 0}, {150, 110, 0}, {160, 110, 0},
                                  {180, 110, 0}, {160, 110, 0}, {180, 110, 0}, {200, 200, 0}};
        if (batch) {
            renderer.points(overlap, 7, 5.0f, Color(0, 255, 0, 90));
            renderer.lines(overlap, 7, 1.0f, Color(0, 0, 255, 90));
            renderer.points(nullptr, 0, 7.0f, Color::WHITE);
            renderer.points(nullptr, std::numeric_limits<Int32>::min(), 7.0f, Color::WHITE);
            renderer.lines(nullptr, 0, 7.0f, Color::WHITE);
            renderer.lines(nullptr, 1, 7.0f, Color::WHITE);
            renderer.lines(nullptr, std::numeric_limits<Int32>::min(), 7.0f, Color::WHITE);
        } else {
            renderer.Renderer::points(overlap, 7, 5.0f, Color(0, 255, 0, 90));
            renderer.Renderer::lines(overlap, 7, 1.0f, Color(0, 0, 255, 90));
        }

        // Exercise buffer resizing and material/VAO changes between batches.
        renderer.setBlendFunc(BlendFunc::None);
        renderer.quadXY(Vector(5, 5), Vector(12, 12), Color::BLUE);
        renderer.quadXY(Vector(22, 5), Vector(12, 12), Vector(0, 0), Vector(1, 1), Material(texture));
        renderer.triangle(Vector(40, 5), Vector(40, 17), Vector(52, 5), Color::GREEN);
        renderer.point(Vector(60, 10), 1.0f, Color::WHITE);
        renderer.line(Vector(70, 5), Vector(80, 17), 1.0f, Color::WHITE);
        D6R_REQUIRE(glGetError() == GL_NO_ERROR);
        return renderer.makeScreenshot();
    }
}

D6R_TEST_CASE("Batched effects match scalar pixels with depth, blending and intervening draws") {
    GraphicsContext context;
    TestRenderer renderer;
    renderer.setViewport(0, 0, 320, 240);
    renderer.setProjectionMatrix(Matrix::orthographic(0, 320, 0, 240, -1, 1));
    renderer.setModelMatrix(Matrix::IDENTITY);
    Image image(2, 2);
    for (Size i = 0; i < 4; ++i) image.at(i) = Color::WHITE;
    const Texture texture = renderer.createTexture(image, TextureFilter::Nearest, true);
    D6R_REQUIRE(texture != Texture());

    for (bool blend: {false, true}) {
        const Image scalar = scene(renderer, texture, false, blend);
        const Image batched = scene(renderer, texture, true, blend);
        Size visible = 0;
        for (Size i = 0; i < scalar.getWidth() * scalar.getHeight(); ++i) {
            D6R_REQUIRE(scalar.at(i) == batched.at(i));
            if (batched.at(i).getRed() || batched.at(i).getGreen() || batched.at(i).getBlue()) ++visible;
        }
        D6R_REQUIRE(visible > 1000);
    }
    renderer.freeTexture(texture);
}
