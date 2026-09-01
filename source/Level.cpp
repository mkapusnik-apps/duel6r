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

#include <queue>
#include <limits>
#include "Game.h"
#include "Level.h"
#include "json/JsonParser.h"
#include "DataException.h"
#include "GameException.h"

namespace Duel6 {
    Level::Level(const std::string &path, bool mirror, const Block::Meta &blockMeta)
            : Level(path, mirror, blockMeta, Math::localRandomSource()) {}

    Level::Level(const std::string &path, bool mirror, const Block::Meta &blockMeta,
                 RandomSource &randomSource)
            : blockMeta(blockMeta), randomSource(randomSource), raisingWater(false) {
        load(path, mirror);
    }

    Level::Level(const std::vector<Uint8> &bytes, bool mirror, const Block::Meta &blockMeta)
            : Level(bytes, mirror, blockMeta, Math::localRandomSource()) {}

    Level::Level(const std::vector<Uint8> &bytes, bool mirror, const Block::Meta &blockMeta,
                 RandomSource &randomSource)
            : blockMeta(blockMeta), randomSource(randomSource), raisingWater(false) {
        load(bytes, mirror);
    }

    void Level::load(const std::string &path, bool mirror) {
        load(Json::Parser().parse(path), mirror);
    }

    void Level::load(const std::vector<Uint8> &bytes, bool mirror) {
        load(Json::Parser().parse(bytes), mirror);
    }

    void Level::load(const Json::Value &root, bool mirror) {
        levelData.clear();

        width = root.get("width").asInt();
        height = root.get("height").asInt();
        background = root.getOrDefault("background", Json::Value::makeString("")).asString();

        constexpr std::int64_t MaximumLevelCells = 4 * 1024 * 1024;
        if (width <= 0 || height <= 0) D6_THROW(DataException, "Level dimensions must be positive");
        const std::int64_t blockCount = static_cast<std::int64_t>(width) * height;
        if (blockCount <= 0 || blockCount > MaximumLevelCells)
            D6_THROW(DataException, "Level dimensions are unsupported");
        Json::Value blocks = root.get("blocks");
        if (blocks.getLength() != static_cast<Size>(blockCount))
            D6_THROW(DataException, "Level block count does not match its dimensions");
        if (blockMeta.empty()
            || blockMeta.size() > static_cast<Size>(std::numeric_limits<Uint16>::max()) + 1u)
            D6_THROW(DataException, "Level block metadata is unavailable");
        levelData.resize(static_cast<Size>(blockCount));
        for (Size i = 0; i < blocks.getLength(); i++) {
            const Int32 block = blocks.get(i).asInt();
            if (block < 0 || static_cast<Size>(block) >= blockMeta.size())
                D6_THROW(DataException, "Level references unavailable block metadata");
            levelData[i] = static_cast<Uint16>(block);
        }

        if (mirror) {
            mirrorLevelData();
        }
        waterBlock = findWaterType();
        waterLevel = findWaterLevel(waterBlock);
    }

    void Level::mirrorLevelData() {
        for (Int32 y = 0; y < height; y++) {
            for (Int32 x = 0; x < width / 2; x++) {
                std::swap(levelData[y * width + x], levelData[y * width + width - 1 - x]);
            }
        }
    }

    void Level::raiseWater() {
        raisingWater = true;
        if (waterLevel < getHeight() - 1) {
            waterLevel++;
            for (Int32 x = 0; x < getWidth(); x++) {
                if (!isWall(x, waterLevel, false)) {
                    setBlock(waterBlock, x, waterLevel);
                }
            }
        }
    }

    Uint16 Level::findWaterType() const {
        for (Int32 y = 0; y < getHeight(); y++) {
            for (Int32 x = 0; x < getWidth(); x++) {
                if (isWater(x, y)) {
                    return getBlock(x, y);
                }
            }
        }

        static Uint16 waterBlocks[] = {4, 16, 33};
        return waterBlocks[Math::random(3, randomSource, "water-type")];
    }

    Int32 Level::findWaterLevel(Uint16 waterBlock) const {
        for (Int32 level = 0; level < getHeight(); level++) {
            for (Int32 x = 0; x < getWidth(); x++) {
                if (!isWall(x, level, true) && getBlock(x, level) != waterBlock) {
                    return level - 1;
                }
            }
        }

        return getHeight() - 1;
    }

    const Water *Level::getWaterType(Int32 x, Int32 y) const {
        return isInside(x,y) ? getBlockMeta(x, y).getWaterType() : Water::NONE;
    }

    bool Level::isPossibleStartingPosition(Int32 x, Int32 y) {
        return isEmpty(x, y) && isWall(x, y - 1, true);
    }

    void Level::findStartingPositions(StartingPositionList &startingPositions) {
        for (Int32 y = 0; y < getHeight(); y++) {
            for (Int32 x = 0; x < getWidth(); x++) {
                if (isPossibleStartingPosition(x, y)) {
                    startingPositions.push_back(std::make_pair(x, y));
                }
            }
        }

        if (startingPositions.empty()) {
            findTopmostNonWallPositions(startingPositions);
            if (startingPositions.empty()) {
                D6_THROW(GameException, "No acceptable starting positions found in this level");
            }
        }
    }

    void Level::findTopmostNonWallPositions(StartingPositionList &startingPositions) {
        for (Int32 y = getHeight() - 1; y >= 0 && startingPositions.empty(); y--) {
            for (Int32 x = 0; x < getWidth(); x++) {
                if (!isWall(x, y, true)) {
                    startingPositions.push_back(std::make_pair(x, y));
                }
            }
        }
    }

    Int32 Level::getWaterLevel() const {
        return waterLevel;
    }

    bool Level::isRaisingWater() const {
        return raisingWater;
    }
}
