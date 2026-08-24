#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include "source/json/JsonParser.h"
#include "source/GameSettings.h"

// GameSettings' weapon collection is not exercised here, but its translation
// unit references Weapon equality. Keep the narrow settings test independent
// of the complete gameplay/renderer link graph.
bool Duel6::Weapon::operator==(const Weapon &weapon) const {
    return impl == weapon.impl;
}

#include "source/GameSettings.cpp"
#include "tests/TestHarness.h"

namespace {
    std::filesystem::path sourceRoot() {
        return std::filesystem::path(D6R_TEST_SOURCE_DIR);
    }

    bool isHexColor(const std::string &value) {
        return value.size() == 6 && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        });
    }

    bool isSoundReference(const Duel6::Json::Value &value) {
        if (value.getType() == Duel6::Json::Value::Type::Null) {
            return true;
        }

        if (value.getType() == Duel6::Json::Value::Type::String) {
            return !value.asString().empty();
        }

        if (value.getType() != Duel6::Json::Value::Type::Array) {
            return false;
        }

        for (Duel6::Size i = 0; i < value.getLength(); ++i) {
            const Duel6::Json::Value item = value.get(i);
            if (item.getType() != Duel6::Json::Value::Type::Null && item.getType() != Duel6::Json::Value::Type::String) {
                return false;
            }
        }

        return true;
    }
}

D6R_TEST_CASE("Burnable Trees defaults on for each fresh application settings instance") {
    Duel6::GameSettings priorSession;
    priorSession.setBurnableTrees(false);

    const Duel6::GameSettings restartedSession;

    D6R_REQUIRE(!priorSession.isBurnableTrees());
    D6R_REQUIRE(restartedSession.isBurnableTrees());
}

D6R_TEST_CASE("Burnable Trees selection remains mutable in the active settings instance") {
    Duel6::GameSettings settings;

    D6R_REQUIRE(settings.isBurnableTrees());
    settings.setBurnableTrees(false);
    D6R_REQUIRE(!settings.isBurnableTrees());
    settings.setBurnableTrees(true);
    D6R_REQUIRE(settings.isBurnableTrees());
}

D6R_TEST_CASE("Block metadata JSON is structurally valid") {
    Duel6::Json::Parser parser;
    Duel6::Json::Value blocks = parser.parse((sourceRoot() / "resources/data/blocks.json").string());

    D6R_REQUIRE(blocks.getType() == Duel6::Json::Value::Type::Array);
    D6R_REQUIRE(blocks.getLength() > 0);

    for (Duel6::Size index = 0; index < blocks.getLength(); ++index) {
        Duel6::Json::Value block = blocks.get(index);
        D6R_REQUIRE(block.getType() == Duel6::Json::Value::Type::Object);
        D6R_REQUIRE(!block.get("kind").asString().empty());
        D6R_REQUIRE(block.get("animations").getType() == Duel6::Json::Value::Type::Array);
        D6R_REQUIRE(block.get("animations").getLength() > 0);
        for (Duel6::Size frame = 0; frame < block.get("animations").getLength(); ++frame) {
            D6R_REQUIRE(block.get("animations").get(frame).asInt() >= 0);
        }
    }
}

D6R_TEST_CASE("Every shipped level JSON matches declared dimensions") {
    Duel6::Json::Parser parser;
    Duel6::Json::Value blockMeta = parser.parse((sourceRoot() / "resources/data/blocks.json").string());
    const Duel6::Size blockCount = blockMeta.getLength();

    std::filesystem::path levelDirectory = sourceRoot() / "resources/levels";
    Duel6::Size levelFileCount = 0;
    for (const auto &entry: std::filesystem::directory_iterator(levelDirectory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }

        ++levelFileCount;
        Duel6::Json::Value level = parser.parse(entry.path().string());
        const Duel6::Int32 width = level.get("width").asInt();
        const Duel6::Int32 height = level.get("height").asInt();
        Duel6::Json::Value blocks = level.get("blocks");

        D6R_REQUIRE(width > 0);
        D6R_REQUIRE(height > 0);
        D6R_REQUIRE(blocks.getType() == Duel6::Json::Value::Type::Array);
        D6R_REQUIRE_EQ(static_cast<Duel6::Size>(width * height), blocks.getLength());
        D6R_REQUIRE(level.get("elevators").getType() == Duel6::Json::Value::Type::Array);

        for (Duel6::Size index = 0; index < blocks.getLength(); ++index) {
            const Duel6::Int32 blockIndex = blocks.get(index).asInt();
            D6R_REQUIRE(blockIndex >= 0);
            D6R_REQUIRE(static_cast<Duel6::Size>(blockIndex) < blockCount);
        }
    }

    D6R_REQUIRE(levelFileCount > 0);
}

D6R_TEST_CASE("Sample profile JSON keeps expected schema") {
    Duel6::Json::Parser parser;
    const std::filesystem::path profileDirectory = sourceRoot() / "resources/profiles/sample";

    Duel6::Json::Value skin = parser.parse((profileDirectory / "skin.json").string());
    D6R_REQUIRE(isHexColor(skin.get("hairTop").asString()));
    D6R_REQUIRE(isHexColor(skin.get("hairBottom").asString()));
    D6R_REQUIRE(isHexColor(skin.get("bodyOuter").asString()));
    D6R_REQUIRE(isHexColor(skin.get("bodyInner").asString()));
    D6R_REQUIRE(isHexColor(skin.get("handOuter").asString()));
    D6R_REQUIRE(isHexColor(skin.get("handInner").asString()));
    D6R_REQUIRE(isHexColor(skin.get("trousers").asString()));
    D6R_REQUIRE(isHexColor(skin.get("shoes").asString()));
    D6R_REQUIRE(isHexColor(skin.get("face").asString()));
    D6R_REQUIRE(isHexColor(skin.get("headBandColor").asString()));
    D6R_REQUIRE(skin.get("hair").asInt() >= 0);
    D6R_REQUIRE(skin.get("headBand").getType() == Duel6::Json::Value::Type::Boolean);

    Duel6::Json::Value sounds = parser.parse((profileDirectory / "sounds.json").string());
    D6R_REQUIRE(isSoundReference(sounds.get("gotHit")));
    D6R_REQUIRE(isSoundReference(sounds.get("wasKilled")));
    D6R_REQUIRE(isSoundReference(sounds.get("hitOther")));
    D6R_REQUIRE(isSoundReference(sounds.get("killedOther")));
    D6R_REQUIRE(isSoundReference(sounds.get("suicide")));
    D6R_REQUIRE(isSoundReference(sounds.get("drowned")));
    D6R_REQUIRE(isSoundReference(sounds.get("pickedBonus")));
}
