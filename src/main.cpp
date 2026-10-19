#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr int kJumpButton = 1;
    constexpr float kFallbackDelta = 1.f / 240.f;

    struct PendingInput {
        bool waiting = false;
        bool releasedBeforeReplay = false;
        bool swallowRelease = false;
        bool releaseNextFrame = false;
        int button = kJumpButton;
        bool isPlayer1 = true;
    };

    enum class PredictionResult {
        Safe,
        Dead,
        Unsupported
    };

    std::array<std::array<PendingInput, 4>, 2> g_pendingInputs{};
    PlayerObject* g_fakePlayer = nullptr;
    bool g_predictionRunning = false;
    bool g_predictionDied = false;
    bool g_predictionUnsupported = false;
    float g_lastDelta = kFallbackDelta;

    int getPredictionFrames() {
        return std::clamp(Mod::get()->getSettingValue<int>("prediction-frames"), 1, 180);
    }

    bool isPredictiveBufferEnabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    PendingInput* getPendingInput(bool isPlayer1, int button) {
        if (button < 0 || button >= 4) return nullptr;
        auto playerIndex = isPlayer1 ? 0u : 1u;
        return &g_pendingInputs[playerIndex][static_cast<size_t>(button)];
    }

    PlayerObject* getPlayer(GJBaseGameLayer* layer, bool isPlayer1) {
        return isPlayer1 ? layer->m_player1 : layer->m_player2;
    }

    bool isSupportedPlayer(PlayerObject* player, PlayLayer* layer, bool isPlayer1) {
        return player
            && layer
            && isPlayer1
            && !layer->m_gameState.m_isDualMode
            && !player->m_isDead
            && !player->m_isPlatformer
            && !player->m_isShip
            && !player->m_isBird
            && !player->m_isBall
            && !player->m_isSpider
            && !player->m_isSwing
            && !player->m_isDashing;
    }

    bool isSupportedStaticCollisionObject(GameObject* object) {
        if (!object || object->m_isDisabled || object->m_isDisabled2) return true;

        // Do not speculate across triggers, grouped/moved objects, or unknown
        // collision types. They require world-state simulation, not just a
        // player trajectory. The whitelist is the ordinary solid/hazard set.
        if (object->m_isTrigger
            || object->m_isActivated
            || object->m_groupCount != 0
            || object->m_hasGroupParent
            || object->m_hasAreaParent
            || object->m_hasAnimatedChild) {
            return false;
        }

        switch (static_cast<int>(object->m_objectType)) {
            case 0:
            case 2:
            case 25:
                return true;
            default:
                return false;
        }
    }

    PredictionResult simulateHeldInput(PlayLayer* layer, PlayerObject* livePlayer, bool isPlayer1, int button, float dt) {
        if (button != kJumpButton
            || !isSupportedPlayer(livePlayer, layer, isPlayer1)
            || !g_fakePlayer
            || g_fakePlayer == livePlayer
            || g_predictionRunning) {
            return PredictionResult::Unsupported;
        }

        auto checkpoint = PlayerCheckpoint::create();
        if (!checkpoint) return PredictionResult::Unsupported;
        if (!std::isfinite(dt) || dt <= 0.f || dt > 0.05f) {
            return PredictionResult::Unsupported;
        }

        livePlayer->saveToCheckpoint(checkpoint);
        g_predictionDied = false;
        g_predictionUnsupported = false;
        g_predictionRunning = true;

        g_fakePlayer->resetCollisionLog(true);
        g_fakePlayer->m_isDead = false;
        g_fakePlayer->loadFromCheckpoint(checkpoint);
        g_fakePlayer->m_isDead = false;
        g_fakePlayer->setPosition(livePlayer->getPosition());
        g_fakePlayer->setVisible(false);
        g_fakePlayer->m_playEffects = false;
        g_fakePlayer->m_hasGroundParticles = false;
        g_fakePlayer->m_hasShipParticles = false;
        g_fakePlayer->removeAllParticles();
        g_fakePlayer->releaseAllButtons();

        float stepDelta = dt;
        int horizon = getPredictionFrames();

        // Match the normal trajectory-probe order: test current collisions,
        // apply the candidate press, then advance the hidden player. The last
        // iteration checks the final predicted position without advancing it.
        for (int frame = 0; frame <= horizon; ++frame) {
            layer->checkCollisions(g_fakePlayer, stepDelta, false);

            if (g_predictionDied || g_fakePlayer->m_isDead) break;
            if (g_predictionUnsupported) break;

            if (frame == 0) {
                // Keep the button down for the entire simulated horizon. This
                // is especially important for wave, where holding determines
                // the player's vertical direction.
                g_fakePlayer->pushButton(static_cast<PlayerButton>(button));
            }
            if (frame == horizon) break;

            g_fakePlayer->update(stepDelta);
            g_fakePlayer->updateRotation(stepDelta);
            g_fakePlayer->updatePlayerScale();

            if (g_predictionDied || g_fakePlayer->m_isDead || g_predictionUnsupported) break;
        }

        g_predictionRunning = false;
        if (g_predictionDied || g_fakePlayer->m_isDead) return PredictionResult::Dead;
        if (g_predictionUnsupported) return PredictionResult::Unsupported;
        return PredictionResult::Safe;
    }

    void resetPendingInputs() {
        for (auto& playerInputs : g_pendingInputs) {
            for (auto& input : playerInputs) input = PendingInput{};
        }
    }

}

class $modify(PredictiveInputBufferPlayLayer, PlayLayer) {
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        if (g_fakePlayer && g_fakePlayer->getParent()) {
            g_fakePlayer->removeFromParentAndCleanup(true);
        }
        g_fakePlayer = nullptr;
        if (m_objectLayer) {
            g_fakePlayer = PlayerObject::create(1, 1, this, this, true);
            if (g_fakePlayer) {
                g_fakePlayer->setVisible(false);
                g_fakePlayer->m_playEffects = false;
                g_fakePlayer->m_hasGroundParticles = false;
                g_fakePlayer->m_hasShipParticles = false;
                m_objectLayer->addChild(g_fakePlayer);
            }
        }
        resetPendingInputs();
    }

    void resetLevel() {
        resetPendingInputs();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        auto fake = g_fakePlayer;
        g_fakePlayer = nullptr;
        resetPendingInputs();
        if (fake && fake->getParent()) fake->removeFromParentAndCleanup(true);
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (g_predictionRunning && player == g_fakePlayer) {
            g_predictionDied = true;
            player->m_isDead = true;
            return;
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(cocos2d::CCPoint position) {
        if (!g_predictionRunning) PlayLayer::playEndAnimationToPos(position);
    }
};

class $modify(PredictiveInputBufferGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_predictionRunning) return;

        if (!isPredictiveBufferEnabled()) {
            flushPendingInputs();
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto pending = getPendingInput(isPlayer1, button);

        if (!down) {
            if (pending && pending->waiting) {
                pending->releasedBeforeReplay = true;
                return;
            }
            if (pending && pending->swallowRelease) {
                pending->swallowRelease = false;
                return;
            }
            if (pending) pending->releaseNextFrame = false;
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (pending && pending->waiting) {
            pending->releasedBeforeReplay = false;
            return;
        }
        if (pending && pending->swallowRelease) pending->swallowRelease = false;
        if (pending && pending->releaseNextFrame) {
            GJBaseGameLayer::handleButton(false, button, isPlayer1);
            pending->releaseNextFrame = false;
        }

        auto layer = PlayLayer::get();
        auto player = getPlayer(this, isPlayer1);
        if (!pending || !layer
            || static_cast<GJBaseGameLayer*>(layer) != this
            || !isSupportedPlayer(player, layer, isPlayer1)) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto result = simulateHeldInput(layer, player, isPlayer1, button, g_lastDelta);
        if (result != PredictionResult::Dead) {
            // Safe or unsupported: preserve vanilla behavior rather than
            // blocking a click on an unmodelled level section.
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        pending->waiting = true;
        pending->releasedBeforeReplay = false;
        pending->button = button;
        pending->isPlayer1 = isPlayer1;
    }

    void update(float dt) {
        g_lastDelta = std::isfinite(dt) && dt > 0.f ? dt : kFallbackDelta;

        if (!isPredictiveBufferEnabled()) flushPendingInputs();

        for (auto& playerInputs : g_pendingInputs) {
            for (auto& input : playerInputs) {
                if (input.releaseNextFrame) {
                    GJBaseGameLayer::handleButton(false, input.button, input.isPlayer1);
                    input.releaseNextFrame = false;
                }

                if (!input.waiting) continue;

                auto layer = PlayLayer::get();
                auto player = getPlayer(this, input.isPlayer1);
                auto result = simulateHeldInput(layer, player, input.isPlayer1, input.button, dt);

                if (result == PredictionResult::Safe) {
                    bool releaseAfterReplay = input.releasedBeforeReplay;
                    input.waiting = false;
                    GJBaseGameLayer::handleButton(true, input.button, input.isPlayer1);
                    input.releaseNextFrame = releaseAfterReplay;
                    continue;
                }

                // Only replay after the probe confirms a safe held-input path.
                // A still-dead or inconclusive result remains queued.
            }
        }

        GJBaseGameLayer::update(dt);
    }

    void flushPendingInputs() {
        for (auto& playerInputs : g_pendingInputs) {
            for (auto& input : playerInputs) {
                if (!input.waiting) continue;
                bool releaseAfterReplay = input.releasedBeforeReplay;
                input.waiting = false;
                GJBaseGameLayer::handleButton(true, input.button, input.isPlayer1);
                input.releaseNextFrame = releaseAfterReplay;
            }
        }
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int objectCount, float dt) {
        if (!g_predictionRunning || player != g_fakePlayer) {
            GJBaseGameLayer::collisionCheckObjects(player, objects, objectCount, dt);
            return;
        }

        if (!objects) {
            g_predictionUnsupported = true;
            return;
        }

        int limit = std::clamp(objectCount, 0, static_cast<int>(objects->size()));
        gd::vector<GameObject*> safeObjects;
        safeObjects.reserve(static_cast<size_t>(limit));
        for (int index = 0; index < limit; ++index) {
            auto object = (*objects)[static_cast<size_t>(index)];
            if (!object || object->m_isDisabled || object->m_isDisabled2) continue;
            if (!isSupportedStaticCollisionObject(object)) {
                g_predictionUnsupported = true;
                return;
            }
            safeObjects.push_back(object);
        }

        GJBaseGameLayer::collisionCheckObjects(
            player, &safeObjects, static_cast<int>(safeObjects.size()), dt
        );
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* object) {
        if (g_predictionRunning && player == g_fakePlayer) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(player, object);
    }

    void playerTouchedRing(PlayerObject* player, RingObject* object) {
        if (g_predictionRunning && player == g_fakePlayer) return;
        GJBaseGameLayer::playerTouchedRing(player, object);
    }

    void playerTouchedTrigger(PlayerObject* player, EffectGameObject* object) {
        if (g_predictionRunning && player == g_fakePlayer) return;
        GJBaseGameLayer::playerTouchedTrigger(player, object);
    }

    void activateSFXTrigger(SFXTriggerGameObject* object) {
        if (!g_predictionRunning) GJBaseGameLayer::activateSFXTrigger(object);
    }

    void activateSongEditTrigger(SongTriggerGameObject* object) {
        if (!g_predictionRunning) GJBaseGameLayer::activateSongEditTrigger(object);
    }

    void gameEventTriggered(GJGameEvent event, int material, int playerID) {
        if (!g_predictionRunning) GJBaseGameLayer::gameEventTriggered(event, material, playerID);
    }
};

class $modify(PredictiveInputBufferEffectObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* layer, int id, const gd::vector<int>* groups) {
        if (!g_predictionRunning) EffectGameObject::triggerObject(layer, id, groups);
    }
};

$on_mod(Loaded) {
    log::info(
        "[PredictiveInputBuffer] Experimental hidden trajectory predictor loaded. "
        "Supported static cube/robot/wave sections are simulated; unsupported areas use vanilla input."
    );
    log::info("[PredictiveInputBuffer] Prediction lookahead: {} game updates", getPredictionFrames());
}
