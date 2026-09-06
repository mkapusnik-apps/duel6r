/*
* Copyright (c) 2006, Ondrej Danek (www.ondrej-danek.net)
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of Ondrej Danek nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <algorithm>
#include "TextureManager.h"
#include "DataException.h"
#include "File.h"
#include "Video.h"
#include "aseprite/animation.h"

namespace Duel6 {
    namespace {
        void validateAnimation(const animation::Animation &animation,
                               const animation::Animation::AnimationView &view) {
            if (&view.animation != &animation)
                D6_THROW(DataException, "Animation view does not belong to the supplied animation");
            if (animation.width == 0 || animation.height == 0 || animation.framesCount == 0
                || animation.frames.size() != animation.framesCount)
                D6_THROW(DataException, "Animation dimensions or frame metadata are invalid");
            if (view.layerViews.size() != animation.layers.size())
                D6_THROW(DataException, "Animation layer view count is invalid");

            for (const auto &layer: animation.layers) {
                if (layer.isGroupLayer) {
                    if (!layer.frames.empty())
                        D6_THROW(DataException, "Animation group layer contains frame data");
                    continue;
                }
                if (layer.frames.size() != animation.framesCount)
                    D6_THROW(DataException, "Animation layer frame count is invalid");
                for (const auto &frame: layer.frames) {
                    if (frame.opacity == 0) continue;
                    if (frame.image >= animation.images.size())
                        D6_THROW(DataException, "Animation cel image is unavailable");
                    const auto &image = animation.images.at(frame.image);
                    const std::size_t pixelCount = static_cast<std::size_t>(image.width) * image.height;
                    if (image.width == 0 || image.height == 0 || image.pixels.size() != pixelCount)
                        D6_THROW(DataException, "Animation cel image dimensions are invalid");
                }
            }
        }
    }

    TextureManager::TextureManager(Renderer &renderer)
            : renderer(renderer) {}

    const animation::Animation TextureManager::loadAnimation(const std::string &path) {
        return animation::Animation::loadAseImage(path);
    }

    Texture TextureManager::generateSprite(const animation::Animation &animation,
                                           const animation::Palette &substitutionTable,
                                           TextureFilter filtering,
                                           bool clamp) const {
        return generateSprite(animation, animation.toView(), substitutionTable, filtering, clamp);
    }

    Texture TextureManager::generateSprite(const animation::Animation &animation,
                                           const animation::Animation::AnimationView &animationView,
                                           const animation::Palette &substitutionTable,
                                           TextureFilter filtering,
                                           bool clamp) const {
        validateAnimation(animation, animationView);
        Image list;
        for (std::size_t f = 0; f < animation.framesCount; ++f) {
            Image frameImage(animation.width, animation.height);
            const std::size_t framePixelCount = static_cast<std::size_t>(animation.width) * animation.height;
            for (std::size_t p = 0; p < framePixelCount; ++p) {
                frameImage.at(p).setAlpha(0);
            }

            for (std::size_t l = 0; l < animation.layers.size(); ++l) {
                const auto &layer = animation.layers.at(l);
                const auto &layerView = animationView.layerViews.at(l);
                if (!layerView.visible || layer.isGroupLayer) continue;
                const animation::Cel &frame = layer.frames.at(f);
                if (frame.opacity == 0) continue;

                const animation::Image &image = animation.images.at(frame.image);
                const float layerOpacity = layer.opacity / 255.0f;
                const auto x = frame.x;
                const auto y = frame.y;
                const auto width = image.width;
                const auto height = image.height;

                auto pixels = image.substitute(substitutionTable, layerOpacity * frame.opacity,
                                               animation.transparentIndex);
                for (uint16_t v = 0; v < height && v + y < animation.height; v++) {
                    if (v + y < 0)
                        continue; // In Aseprite image cel can be placed in a way that it spans beyond the image boundaries (and could crash code below)
                    for (uint16_t u = 0; u < width && u + x < animation.width; u++) {
                        if (u + x < 0)
                            continue;
                        //TODO LAYER BLEND MODE
                        const auto &color = pixels.at(static_cast<std::size_t>(v) * width + u);
                        auto &dstColor = frameImage.at((y + v) * animation.width + u + x);
                        bool transparent = dstColor.getRed() == 0 &&
                                           dstColor.getGreen() == 0 &&
                                           dstColor.getBlue() == 0 &&
                                           dstColor.getAlpha() == 0;
                        if (color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0) {
                            // do not overwrite pixel with empty pixel
                            continue;
                        } else if (transparent) {
                            // overwrite empty pixel
                            dstColor.set(color.r, color.g, color.b, color.a);
                        } else {
                            // color mixing //TODO
                            switch (layer.blendMode) {
                                case animation::Layer::BLEND_MODE::Darken: {
                                    uint8_t R = std::min(dstColor.getRed(), color.r);
                                    uint8_t G = std::min(dstColor.getGreen(), color.g);
                                    uint8_t B = std::min(dstColor.getBlue(), color.b);
                                    uint8_t A = std::max(dstColor.getAlpha(), color.a);
                                    R += (R - dstColor.getRed()) * ((255 - color.a) / 255.0f);
                                    G += (G - dstColor.getGreen()) * ((255 - color.a) / 255.0f);
                                    B += (B - dstColor.getBlue()) * ((255 - color.a) / 255.0f);

                                    dstColor.set(R, G, B, A);
                                    break;
                                }
                                case animation::Layer::BLEND_MODE::Normal:
                                default:
                                    dstColor.add(color.r, color.g, color.b, color.a);
                            }
                        }
                    }
                }
            }
            // frameImage now contains pixels from all layers
            list.addSlice(frameImage);
        }
        return renderer.createTexture(list, filtering, clamp);
    }

    Texture TextureManager::loadStack(const std::string &path, TextureFilter filtering, bool clamp) {
        SubstitutionTable emptySubstitutionTable;
        return loadStack(path, filtering, clamp, emptySubstitutionTable);
    }

    Texture TextureManager::loadStack(const std::string &path, TextureFilter filtering, bool clamp,
                                      const SubstitutionTable &substitutionTable) {
        Image image = Image::loadStack(path);
        substituteColors(image, substitutionTable);

        Texture texture = renderer.createTexture(image, filtering, clamp);
        return texture;
    }

    const TextureDictionary TextureManager::loadDict(const std::string &path, TextureFilter filtering, bool clamp) {
        std::vector<std::string> textureFiles = File::listDirectory(path);

        TextureDictionary dict;
        for (std::string &file : textureFiles) {
            Image image = Image::load(path + file);
            Texture texture = renderer.createTexture(image, filtering, clamp);
            dict.textures[file] = texture;
        }

        return dict;
    }

    void TextureManager::dispose(Texture texture) {
        renderer.freeTexture(texture);
    }

    void TextureManager::substituteColors(Image &image, const SubstitutionTable &substitutionTable) {
        if (substitutionTable.empty()) {
            return;
        }

        Size imgSize = image.getWidth() * image.getHeight() * image.getDepth();
        for (Size i = 0; i < imgSize; ++i) {
            Color &color = image.at(i);
            auto substituteColor = substitutionTable.find(color);
            if (substituteColor != substitutionTable.end()) {
                color = substituteColor->second;
            }
        }
    }
}
