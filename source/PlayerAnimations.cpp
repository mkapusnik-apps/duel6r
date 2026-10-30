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

#include <string>
#include "DataException.h"
#include "PlayerAnimations.h"
#ifndef D6R_HEADLESS_CORE
#include "PlayerSkinColors.h"
#include "TextureManager.h"
#include "aseprite/animation.h"
#endif

namespace Duel6 {
    Animation defaultPlayerAnimation(PlayerPose pose) {
        static const AnimationEntry stand[] = {0,2000,1,5000,2,2000,3,500,4,200,5,500,6,100,7,500,8,500,9,500,10,100,-1,0};
        static const AnimationEntry hit[] = {0,500,-1,0};
        static const AnimationEntry walk[] = {0,50,1,50,2,50,3,50,-1,0};
        static const AnimationEntry air[] = {0,200,1,200,2,200,-1,0};
        static const AnimationEntry duck[] = {0,3000,1,100,2,1000,3,2000,4,50,5,1000,6,100,7,100,8,50,-1,0};
        static const AnimationEntry deadFall[] = {0,100,1,100,2,100,3,100,-1,0};
        static const AnimationEntry deadHit[] = {0,100,1,50,2,50,3,50,4,50,5,50,6,50,7,100,8,120,9,1000,-1,0};
        static const AnimationEntry deadLying[] = {0,1000,-1,0};
        static const AnimationEntry dying[] = {0,100,1,100,2,100,3,100,4,100,5,50,6,100,7,50,8,50,9,50,10,50,11,50,12,50,13,100,14,120,15,1000,-1,0};
        static const AnimationEntry pick[] = {0,100,1,100,2,100,3,100,4,100,5,100,6,100,7,100,8,100,9,100,10,100,-1,0};
        switch (pose) {
            case PlayerPose::Stand: return stand;
            case PlayerPose::HitStand: case PlayerPose::HitDuck: return hit;
            case PlayerPose::Walk: return walk;
            case PlayerPose::Fall: case PlayerPose::Jump: return air;
            case PlayerPose::Duck: return duck;
            case PlayerPose::DeadFall: return deadFall;
            case PlayerPose::DeadHit: return deadHit;
            case PlayerPose::DeadLying: return deadLying;
            case PlayerPose::Dying: return dying;
            case PlayerPose::Pick: return pick;
        }
        D6_THROW(DataException, "Invalid player pose");
    }
#ifndef D6R_HEADLESS_CORE
    namespace {
        const std::vector<AnimationEntry> &requiredAnimation(const animation::Animation &animation,
                                                              const std::string &name) {
            const auto lookup = animation.animationLookup.find(name);
            if (lookup == animation.animationLookup.end() || lookup->second >= animation.animations.size())
                D6_THROW(DataException, "Required player animation is unavailable: " + name);
            const auto &entries = animation.animations.at(lookup->second);
            if (entries.size() < 2 || entries.size() % 2 != 0)
                D6_THROW(DataException, "Player animation entries are invalid: " + name);
            for (std::size_t index = 0; index < entries.size(); index += 2) {
                const AnimationEntry frame = entries.at(index);
                const AnimationEntry duration = entries.at(index + 1);
                if (frame == -1) {
                    if (duration != 0 || index + 2 != entries.size())
                        D6_THROW(DataException, "Player animation terminator is invalid: " + name);
                    return entries;
                }
                if (frame < 0 || frame >= animation.framesCount || duration <= 0)
                    D6_THROW(DataException, "Player animation frame is invalid: " + name);
            }
            D6_THROW(DataException, "Player animation terminator is missing: " + name);
        }
    }

    PlayerAnimation::PlayerAnimation(const animation::Animation &animation, const std::string &name)
            : animation(requiredAnimation(animation, name)) {
    }

    Animation PlayerAnimation::get() const {
        return animation.data();
    }

    PlayerAnimations::PlayerAnimations(const animation::Animation &animation)
            : animation(animation),
              stand(animation, "Stand"),
              hitStand(animation, "HIT_Stand"),
              walk(animation, "Walk"),
              fall(animation, "Fall"),
              jump(animation, "Jump"),
              duck(animation, "Duck"),
              hitDuck(animation, "HIT_Duck"),
              deadFall(animation, "Dead_fall"),
              deadHit(animation, "Dead_hit"),
              deadLying(animation, "Dead_lying"),
              dying(animation, "Death"),
              pick(animation, "Pick") {}

    Texture PlayerAnimations::generateAnimationTexture(const TextureManager &textureManager,
                                                       const PlayerSkinColors &colors) const {
        animation::Palette substitution_table(animation.palette);
        int dst[] = {4, 5, 8, 9, 12, 13, 16, 20, 24, 36, 37}; //indexes to palette colors used in the man.ase
        int src[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        auto &substitution = substitution_table.colors;
        for (int i = 0; i < 11; i++) {
            const auto &color = colors.get((PlayerSkinColors::BodyPart) src[i]);
            substitution[dst[i]].r = color.getRed();
            substitution[dst[i]].g = color.getGreen();
            substitution[dst[i]].b = color.getBlue();
            substitution[dst[i]].a = color.getAlpha();
        }

        auto view = animation.toView();

        view.setLayerVisibility("Hair", colors.getHair() == PlayerSkinColors::Hair::Normal);
        view.setLayerVisibility("Hair_Short", colors.getHair() == PlayerSkinColors::Hair::Short);
        view.setLayerVisibility("Headband", colors.hasHeadBand());

        return textureManager.generateSprite(animation, view, substitution_table, TextureFilter::Nearest, true);
    }

    const PlayerAnimation &PlayerAnimations::getStand() const {
        return stand;
    }

    const PlayerAnimation &PlayerAnimations::getHitStand() const {
        return hitStand;
    }

    const PlayerAnimation &PlayerAnimations::getWalk() const {
        return walk;
    }

    const PlayerAnimation &PlayerAnimations::getFall() const {
        return fall;
    }

    const PlayerAnimation &PlayerAnimations::getJump() const {
        return jump;
    }

    const PlayerAnimation &PlayerAnimations::getDuck() const {
        return duck;
    }

    const PlayerAnimation &PlayerAnimations::getHitDuck() const {
        return hitDuck;
    }

    const PlayerAnimation &PlayerAnimations::getDeadFall() const {
        return deadFall;
    }

    const PlayerAnimation &PlayerAnimations::getDeadHit() const {
        return deadHit;
    }

    const PlayerAnimation &PlayerAnimations::getDeadLying() const {
        return deadLying;
    }

    const PlayerAnimation &PlayerAnimations::getDying() const {
        return dying;
    }

    const PlayerAnimation &PlayerAnimations::getPick() const {
        return pick;
    }

    Animation PlayerAnimations::forPose(PlayerPose pose) const {
        switch (pose) {
            case PlayerPose::Stand: return getStand().get();
            case PlayerPose::HitStand: return getHitStand().get();
            case PlayerPose::Walk: return getWalk().get();
            case PlayerPose::Fall: return getFall().get();
            case PlayerPose::Jump: return getJump().get();
            case PlayerPose::Duck: return getDuck().get();
            case PlayerPose::HitDuck: return getHitDuck().get();
            case PlayerPose::DeadFall: return getDeadFall().get();
            case PlayerPose::DeadHit: return getDeadHit().get();
            case PlayerPose::DeadLying: return getDeadLying().get();
            case PlayerPose::Dying: return getDying().get();
            case PlayerPose::Pick: return getPick().get();
        }
        D6_THROW(DataException, "Invalid player pose");
    }
#endif
}
