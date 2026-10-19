#include <Geode/Geode.hpp>
#include <Geode/utils/Keyboard.hpp>

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

    int currentStage = 0;

    void logStage() {
        auto const& stage = stages[currentStage];

        if (!stage.enabled) {
            log::info("[InputBuffer] Stage 5: OFF (100% Raw Input)");
            return;
        }

        log::info(
            "[InputBuffer] Stage {}: {} Ticks All | {} Ticks Wave",
            currentStage + 1,
            stage.allTicks,
            stage.waveTicks
        );
    }

    void nextStage() {
        currentStage = (currentStage + 1) % 5;
        logStage();
    }
}

$on_mod(Loaded) {
    KeyboardInputEvent().listen([](KeyboardInputData& data) {
        if (data.key == enumKeyCodes::KEY_F6
            && data.action == KeyboardInputData::Action::Press) {
            nextStage();
        }
        return ListenerResult::Propagate;
    });

    log::info("[InputBuffer] Prototype loaded. Press F6 to cycle stages.");
    logStage();
}
