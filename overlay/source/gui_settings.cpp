#include "gui_settings.hpp"

#include "elm_overlayframe.hpp"

#include <array>
#include <cstdio>
#include <string>

namespace {

    /* Selectable wake delays, in milliseconds. */
    constexpr std::array WAKE_DELAYS{0u, 500u, 1000u, 1500u, 2000u, 3000u, 5000u};

    std::string FormatDelay(u32 ms) {
        if (ms == 0) {
            return "Instant";
        }

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u.%us", ms / 1000, (ms % 1000) / 100);
        return buf;
    }

    /* Nearest entry in WAKE_DELAYS, so a hand edited config still lands somewhere. */
    size_t FindDelayIndex(u32 ms) {
        size_t best = 0;
        u32 best_diff = UINT32_MAX;

        for (size_t i = 0; i < WAKE_DELAYS.size(); i++) {
            const u32 diff = WAKE_DELAYS[i] > ms ? WAKE_DELAYS[i] - ms : ms - WAKE_DELAYS[i];
            if (diff < best_diff) {
                best_diff = diff;
                best = i;
            }
        }

        return best;
    }

    /* A row of the behaviour page: a situation, and what actually happens. */
    void AddAnswer(tsl::elm::List *list, const char *situation, bool plays) {
        auto item = new tsl::elm::ListItem(situation);
        item->setValue(plays ? "Plays" : "Silent", !plays);
        list->addItem(item);
    }

    void AddNotes(tsl::elm::List *list, const std::string &text, u16 height) {
        list->addItem(new tsl::elm::CustomDrawer([text](tsl::gfx::Renderer *renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString(text.c_str(), false, x, y + 20, 15,
                                 tsl::gfx::Renderer::a(tsl::style::color::ColorDescription), w);
        }), height);
    }

}

tsl::elm::Element *SettingsGui::createUI() {
    auto frame = new SysTuneOverlayFrame();
    auto list  = new tsl::elm::List();

    /* Read the settings the sysmodule is actually running with. */
    bool home_menu_only = true;
    bool focus_detect = true;
    bool focus_detect_available = false;
    bool autoplay = true;
    bool pause_on_sleep = true;
    bool resume_on_wake = true;
    bool pause_on_unplug = true;
    u32 wake_delay = 1500;

    tuneGetHomeMenuOnly(&home_menu_only);
    tuneGetFocusDetect(&focus_detect);
    tuneGetFocusDetectAvailable(&focus_detect_available);
    tuneGetAutoPlay(&autoplay);
    tuneGetPauseOnSleep(&pause_on_sleep);
    tuneGetResumeOnWake(&resume_on_wake);
    tuneGetPauseOnHeadphoneUnplug(&pause_on_unplug);
    tuneGetWakeDelayMs(&wake_delay);

    list->addItem(new tsl::elm::CategoryHeader("Home menu music"));

    /* Pause as soon as a game takes over. */
    auto home_only_toggle = new tsl::elm::ToggleListItem("Home menu only", home_menu_only, "On", "Off");
    home_only_toggle->setStateChangedListener([](bool new_value) {
        tuneSetHomeMenuOnly(new_value);
    });
    list->addItem(home_only_toggle);

    /* Keep playing when a game is only suspended behind the home menu. */
    auto focus_toggle = new tsl::elm::ToggleListItem("Play over suspended games", focus_detect, "On", "Off");
    focus_toggle->setStateChangedListener([](bool new_value) {
        tuneSetFocusDetect(new_value);
    });
    if (!focus_detect_available) {
        /* pdm:qry could not be opened, so this cannot do anything. Say so. */
        focus_toggle->setValue("Unavailable", true);
    }
    list->addItem(focus_toggle);

    /* Start playing on boot. */
    auto autoplay_toggle = new tsl::elm::ToggleListItem("Start at boot", autoplay, "On", "Off");
    autoplay_toggle->setStateChangedListener([](bool new_value) {
        tuneSetAutoPlay(new_value);
    });
    list->addItem(autoplay_toggle);

    list->addItem(new tsl::elm::CategoryHeader("Sleep"));

    auto sleep_toggle = new tsl::elm::ToggleListItem("Pause on sleep", pause_on_sleep, "On", "Off");
    sleep_toggle->setStateChangedListener([](bool new_value) {
        tuneSetPauseOnSleep(new_value);
    });
    list->addItem(sleep_toggle);

    auto wake_toggle = new tsl::elm::ToggleListItem("Play on lock screen", resume_on_wake, "On", "Off");
    wake_toggle->setStateChangedListener([](bool new_value) {
        tuneSetResumeOnWake(new_value);
    });
    list->addItem(wake_toggle);

    /* Cycles through presets rather than using a slider, so the value is readable. */
    auto delay_item = new tsl::elm::ListItem("Wake delay");
    delay_item->setValue(FormatDelay(wake_delay));
    delay_item->setClickListener([delay_item, index = FindDelayIndex(wake_delay)](u64 keys) mutable {
        if (keys & HidNpadButton_A) {
            index = (index + 1) % WAKE_DELAYS.size();
            tuneSetWakeDelayMs(WAKE_DELAYS[index]);
            delay_item->setValue(FormatDelay(WAKE_DELAYS[index]));
            return true;
        }
        return false;
    });
    list->addItem(delay_item);

    list->addItem(new tsl::elm::CategoryHeader("Headphones"));

    auto unplug_toggle = new tsl::elm::ToggleListItem("Pause when unplugged", pause_on_unplug, "On", "Off");
    unplug_toggle->setStateChangedListener([](bool new_value) {
        tuneSetPauseOnHeadphoneUnplug(new_value);
    });
    list->addItem(unplug_toggle);

    list->addItem(new tsl::elm::CategoryHeader("Help"));

    auto behaviour_button = new tsl::elm::ListItem("Where does music play?");
    behaviour_button->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<BehaviourGui>();
            return true;
        }
        return false;
    });
    list->addItem(behaviour_button);

    frame->setContent(list);

    return frame;
}

tsl::elm::Element *BehaviourGui::createUI() {
    auto frame = new SysTuneOverlayFrame();
    auto list  = new tsl::elm::List();

    bool home_menu_only = true;
    bool focus_detect = true;
    bool focus_detect_available = false;
    bool resume_on_wake = true;
    bool pause_on_unplug = true;

    tuneGetHomeMenuOnly(&home_menu_only);
    tuneGetFocusDetect(&focus_detect);
    tuneGetFocusDetectAvailable(&focus_detect_available);
    tuneGetResumeOnWake(&resume_on_wake);
    tuneGetPauseOnHeadphoneUnplug(&pause_on_unplug);

    /* Focus detection only does anything when pdm:qry is available. */
    const bool focus = focus_detect && focus_detect_available;

    /* Under "home menu only" a game is silent unless that title was turned */
    /* on by hand, which is the per game toggle on the main page. */
    const bool plays_in_games = !home_menu_only;

    list->addItem(new tsl::elm::CategoryHeader("With your current settings"));

    AddAnswer(list, "Home menu", true);
    AddAnswer(list, "Settings, eShop, Album", true);
    AddAnswer(list, "Playing a game", plays_in_games);
    AddAnswer(list, "HOME over a game", plays_in_games || focus);
    AddAnswer(list, "Lock screen", resume_on_wake && (plays_in_games || focus));
    AddAnswer(list, "Console asleep", false);
    AddAnswer(list, "Headphones unplugged", !pause_on_unplug);

    list->addItem(new tsl::elm::CategoryHeader("Notes"));

    std::string notes =
        "Sleep: the console cuts audio while it sleeps, so nothing can play "
        "until it wakes up again.\n \n"
        "Lock screen: the wake up screen keeps your game suspended, so it "
        "counts as the home menu.\n \n";

    if (!focus_detect_available) {
        notes +=
            "Suspended game detection is unavailable on this console, so a "
            "game that is only suspended still counts as in game.\n \n";
    }

    notes += "The per game toggle on the main page overrides everything here.";

    AddNotes(list, notes, 260);

    frame->setContent(list);

    return frame;
}
