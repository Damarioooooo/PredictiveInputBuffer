#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/utils/Keyboard.hpp>
#include <algorithm>
#include <deque>

using namespace geode::prelude;

namespace {
    struct Stage {
        int allTicks;
        int waveTicks;
        bool enabled;
    };

    constexpr Stage stages[] = {
        {2, 2, true},
        {4, 4, true},
        {8, 8, true},
        {16, 8, true},
        {0, 0, false}
    };

    struct BufferedInput {
        bool down;
        int button;
        bool isPlayer1;
        double dueAt;
    };

    std::deque<BufferedInput> g_inputs;
    double g_levelTime = 0.0;

    int getCurrentStage() {
        return std::clamp(Mod::get()->getSettingValue<int>("buffer-stage"), 0, 4);
    }

    void logStage() {
        int index = getCurrentStage();
        auto const& stage = stages[index];

        if (!stage.enabled) {
            log::info("[InputBuffer] Stage 5: OFF (raw input)");
            return;
        }

        log::info(
            "[InputBuffer] Stage {}: {} ticks all | {} ticks wave",
            index + 1,
            stage.allTicks,
            stage.waveTicks
        );
    }

    void nextStage() {
        int next = (getCurrentStage() + 1) % 5;
        Mod::get()->setSettingValue<int>("buffer-stage", next);
        logStage();
    }
}

class $modify(PredictiveInputBufferGameLayer, GJBaseGameLayer) {
    bool init() {
        if (!GJBaseGameLayer::init()) return false;
        g_inputs.clear();
        g_levelTime = 0.0;
        return true;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        int stageIndex = getCurrentStage();
        auto const& stage = stages[stageIndex];
        if (!stage.enabled) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto player = isPlayer1 ? m_player1 : m_player2;
        bool isWave = player && player->m_isDart;
        int ticks = isWave ? stage.waveTicks : stage.allTicks;
        if (ticks <= 0) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        g_inputs.push_back({
            down,
            button,
            isPlayer1,
            g_levelTime + static_cast<double>(ticks) / 240.0
        });
    }

    void update(float dt) {
        g_levelTime += dt;
        while (!g_inputs.empty() && g_inputs.front().dueAt <= g_levelTime) {
            auto input = g_inputs.front();
            g_inputs.pop_front();
            GJBaseGameLayer::handleButton(input.down, input.button, input.isPlayer1);
        }
        GJBaseGameLayer::update(dt);
    }
};

$on_mod(Loaded) {
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.key == enumKeyCodes::KEY_F6
            && data.action == KeyboardInputData::Action::Press) {
            nextStage();
        }
        return ListenerResult::Propagate;
    });

    log::info("[InputBuffer] Loaded. Set the stage in mod settings or press F6 with a keyboard.");
    logStage();
}
