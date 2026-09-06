/*
 * Aseprite to animation transformation
 * Version 0.1
 * Copyright 2018 by Frantisek Veverka
 *
 */

#ifndef ASEPRITE_TO_ANIMATION_H_
#define ASEPRITE_TO_ANIMATION_H_
#include "aseprite.h"
#include "animation.h"
#include <stdexcept>

animation::LoopType from(uint16_t type) {
    switch (type) {
    case 0:
        return animation::LoopType::FORWARD;
    case 1:
        return animation::LoopType::REVERSE;
    case 2:
        return animation::LoopType::PING_PONG;
    default:
        return animation::LoopType::FORWARD;
    }
}
std::vector<uint8_t> from(const std::vector<aseprite::PIXEL_DATA> & in) {
    std::vector<uint8_t> result(in.size());
    for (size_t i = 0; i < in.size(); i++) {
        result[i] = in[i].INDEXED;
    }
    return result;
}
animation::Animation fromASEPRITE(const aseprite::ASEPRITE & ase) {
    animation::Animation animation;
    animation.width = ase.header.width;
    animation.height = ase.header.height;
    animation.framesCount = ase.header.frames;
    animation.transparentIndex = ase.header.transparentIndex;
    if (animation.width == 0 || animation.height == 0 || animation.framesCount == 0
        || ase.frames.size() != animation.framesCount)
        throw std::runtime_error("Aseprite animation dimensions or frame count are invalid");
    animation.frames.resize(animation.framesCount);

    for (const auto & chunk : ase.frames[0].chunks) {
        if (chunk.type == 0x2019) {
            const auto & palette_chunk = std::get<aseprite::PALETTE_CHUNK>(chunk.data);
            if (palette_chunk.colors.size() > animation.palette.colors.size())
                throw std::runtime_error("Aseprite palette is too large");
            for (size_t i = 0; i < palette_chunk.colors.size(); i++) {
                animation.palette.colors.at(i).r = palette_chunk.colors[i].r;
                animation.palette.colors.at(i).g = palette_chunk.colors[i].g;
                animation.palette.colors.at(i).b = palette_chunk.colors[i].b;
                animation.palette.colors.at(i).a = palette_chunk.colors[i].a;

            }

        }
        if (chunk.type == 0x2018) {
            const auto & tag_chunk = std::get<aseprite::TAG_CHUNK>(chunk.data);
            animation.loops.reserve(tag_chunk.tags.size());
            for (const auto & tag : tag_chunk.tags) {
                animation.loops.emplace_back(
                    tag.from,
                    tag.to,
                    from(tag.direction),
                    tag.name.toString()
                    );
            }
        }
        if (chunk.type == 0x2004) {
            const auto & layer = std::get<aseprite::LAYER_CHUNK>(chunk.data);
            animation.layers.emplace_back(
                animation::Layer::BLEND_MODE(layer.blendMode),
                layer.flags & 0x1,
                layer.layerType == 1,
                layer.opacity,
                layer.name.toString(),
                animation.framesCount);
        }
    }
    for (size_t f = 0; f < animation.framesCount; f++) {
        animation.frames[f].duration = ase.frames[f].duration;
        for (const auto & chunk : ase.frames[f].chunks) {
            if (chunk.type == 0x2005) {
                const auto & cel_chunk = std::get<aseprite::CEL_CHUNK>(chunk.data);
                if (cel_chunk.layerIndex >= animation.layers.size())
                    throw std::runtime_error("Aseprite cel layer index is invalid");
                auto & layer = animation.layers.at(cel_chunk.layerIndex);
                if (layer.isGroupLayer || f >= layer.frames.size())
                    throw std::runtime_error("Aseprite cel references an invalid layer frame");
                auto & cel = layer.frames.at(f);
                if (cel_chunk.type == 1) { // linked cel
                    if (cel_chunk.frameLink >= layer.frames.size())
                        throw std::runtime_error("Aseprite linked cel frame is invalid");
                    const auto & linkedCel = layer.frames.at(cel_chunk.frameLink);
                    cel.image = linkedCel.image;
                    cel.opacity = linkedCel.opacity;
                    cel.x = linkedCel.x;
                    cel.y = linkedCel.y;

                } else {
                    cel.x = cel_chunk.x;
                    cel.y = cel_chunk.y;
                    cel.opacity = cel_chunk.opacity;
                    animation.images.emplace_back(
                        cel_chunk.width,
                        cel_chunk.height,
                        from(cel_chunk.pixels));
                    cel.image = animation.images.size() - 1;
                }
            }
        }
    }
    for (const auto & loop : animation.loops) {
        if (loop.from > loop.to || loop.to >= animation.framesCount)
            throw std::runtime_error("Aseprite animation loop range is invalid");
        std::vector<int32_t> animationLoop;

        if (loop.loopType == animation::LoopType::FORWARD) {
            uint16_t loopLength = loop.to - loop.from + 1;
            animationLoop.reserve(loopLength * 2 + 2); // format is frame,duration,...,frame,duraion,-1,0

            for (uint16_t frame = loop.from; frame <= /*(!)*/loop.to; frame++) {
                animationLoop.push_back(frame);
                animationLoop.push_back(animation.frames[frame].duration);
            }

        } else if (loop.loopType == animation::LoopType::PING_PONG) {
            uint16_t loopLength = loop.to - loop.from + 1;
            animationLoop.reserve(loopLength * 4); // format is frame,duration,...,frame,duraion,-1,0

            for (uint16_t frame = loop.from; frame < /*(!)*/loop.to; frame++) {
                animationLoop.push_back(frame);
                animationLoop.push_back(animation.frames[frame].duration);
            }
            for (int32_t frame = loop.to; frame >= loop.from; --frame) {
                animationLoop.push_back(frame);
                animationLoop.push_back(animation.frames[frame].duration);
            }

        } else if (loop.loopType == animation::LoopType::REVERSE) {
            uint16_t loopLength = loop.to - loop.from + 1;
            animationLoop.reserve(loopLength * 2 + 2); // format is frame,duration,...,frame,duraion,-1,0

            for (int32_t frame = loop.to; frame >= loop.from; --frame) {
                animationLoop.push_back(frame);
                animationLoop.push_back(animation.frames[frame].duration);
            }
        }

        animationLoop.push_back(-1);
        animationLoop.push_back(0);
        animation.animations.push_back(animationLoop);
        animation.animationLookup[loop.name] = animation.animations.size() - 1;
    }
    return animation;
}

#endif /* ASEPRITE_TO_ANIMATION_H_ */
