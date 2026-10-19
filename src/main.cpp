#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <algorithm>
#include <array>

using namespace geode::prelude;

namespace {
    constexpr int kMaxBufferedFrames = 16;
    constexpr int kTrackedButtons = 4; // Game button IDs 0-3; ID 0 is unused.

    struct PendingInput {
        bool waiting = false;
        bool releasedBeforeReplay = false;
        bool swallowRelease = false;
        bool releaseNextFrame = false;
        int framesRemaining = 0;
        int button = 0;
        bool isPlayer1 = true;
    };

    std::array<std::array<PendingInput, kTrackedButtons>, 2> g_pendingInputs{};

    int getBufferFrames() {
        return std::clamp(Mod::get()->getSettingValue<int>("buffer-frames"), 0, kMaxBufferedFrames);
    }

    PendingInput* getPendingInput(bool isPlayer1, int button) {
        if (button < 0 || button >= kTrackedButtons) return nullptr;
        auto playerIndex = isPlayer1 ? 0u : 1u;
        return &g_pendingInputs[playerIndex][static_cast<size_t>(button)];
    }

    PlayerObject* getPlayer(GJBaseGameLayer* layer, bool isPlayer1) {
        return isPlayer1 ? layer->m_player1 : layer->m_player2;
    }

    // Cube and robot jumps require ground contact. The other modes accept
    // clicks in the air, so their inputs must pass through immediately.
    bool isGroundJumpMode(PlayerObject* player) {
        return player
            && !player->m_isDead
            && !player->m_isShip
            && !player->m_isBird
            && !player->m_isBall
            && !player->m_isDart
            && !player->m_isSpider
            && !player->m_isSwing;
    }

    void logBufferSetting() {
        int frames = getBufferFrames();
        if (frames == 0) {
            log::info("[InputBuffer] Early-jump buffer OFF");
        } else {
            log::info("[InputBuffer] Buffering early ground-jump presses for up to {} frames", frames);
        }
    }

    void nextBufferSetting() {
        int next = (getBufferFrames() + 1) % (kMaxBufferedFrames + 1);
        Mod::get()->setSettingValue<int>("buffer-frames", next);
        logBufferSetting();
    }

    void resetPendingInputs() {
        for (auto& playerInputs : g_pendingInputs) {
            for (auto& input : playerInputs) {
                input = PendingInput{};
            }
        }
    }
}

class $modify(PredictiveInputBufferGameLayer, GJBaseGameLayer) {
    bool init() {
        if (!GJBaseGameLayer::init()) return false;
        resetPendingInputs();
        return true;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        auto pending = getPendingInput(isPlayer1, button);

        if (!down) {
            if (pending && pending->waiting) {
                // Preserve a quick tap: replay down on landing, then release next frame.
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

        if (pending && pending->waiting) return;
        if (pending && pending->swallowRelease) pending->swallowRelease = false;
        if (pending && pending->releaseNextFrame) {
            GJBaseGameLayer::handleButton(false, button, isPlayer1);
            pending->releaseNextFrame = false;
        }

        auto player = getPlayer(this, isPlayer1);
        int frames = getBufferFrames();
        if (!pending || frames == 0 || !isGroundJumpMode(player) || player->m_isOnGround) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        pending->waiting = true;
        pending->releasedBeforeReplay = false;
        pending->framesRemaining = frames;
        pending->button = button;
        pending->isPlayer1 = isPlayer1;
    }

    void update(float dt) {
        for (auto& playerInputs : g_pendingInputs) {
            for (auto& input : playerInputs) {
                if (input.releaseNextFrame) {
                    GJBaseGameLayer::handleButton(false, input.button, input.isPlayer1);
                    input.releaseNextFrame = false;
                }

                if (!input.waiting) continue;

                auto player = getPlayer(this, input.isPlayer1);
                if (!isGroundJumpMode(player)) {
                    input.waiting = false;
                    input.swallowRelease = !input.releasedBeforeReplay;
                    continue;
                }

                if (player->m_isOnGround) {
                    bool releaseAfterReplay = input.releasedBeforeReplay;
                    input.waiting = false;
                    GJBaseGameLayer::handleButton(true, input.button, input.isPlayer1);
                    input.releaseNextFrame = releaseAfterReplay;
                    continue;
                }

                --input.framesRemaining;
                if (input.framesRemaining <= 0) {
                    input.waiting = false;
                    input.swallowRelease = !input.releasedBeforeReplay;
                }
            }
        }

        GJBaseGameLayer::update(dt);
    }
};

$on_mod(Loaded) {
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.key == enumKeyCodes::KEY_F6
            && data.action == KeyboardInputData::Action::Press) {
            nextBufferSetting();
        }
        return ListenerResult::Propagate;
    });

    log::info("[InputBuffer] Loaded. Choose the early-jump buffer window in mod settings; F6 adjusts it with a keyboard.");
    logBufferSetting();
}
