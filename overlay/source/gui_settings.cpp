#include "gui_settings.hpp"

#include "elm_overlayframe.hpp"
#include "elm_volume.hpp"
#include "config/config.hpp"
#include "sdmc/sdmc.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

    constexpr size_t num_steps = 20;

    /* Selectable wake delays, in milliseconds. */
    constexpr std::array WAKE_DELAYS{0u, 500u, 1000u, 1500u, 2000u, 3000u, 5000u};

    /* Selectable fade lengths, in milliseconds. */
    constexpr std::array FADE_LENGTHS{0u, 200u, 400u, 800u, 1500u, 3000u};

    constexpr std::array SupportedTypes{
#ifdef WANT_MP3
        ".mp3",
#endif
#ifdef WANT_FLAC
        ".flac",
#endif
#ifdef WANT_WAV
        ".wav",
        ".wave",
#endif
    };

    bool SupportsType(const char *name) {
        const auto ext = std::strrchr(name, '.');
        if (!ext) {
            return false;
        }

        for (auto &type : SupportedTypes) {
            if (strcasecmp(ext, type) == 0) {
                return true;
            }
        }

        return false;
    }

    std::string FormatDelay(u32 ms) {
        if (ms == 0) {
            return "Instant";
        }

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u.%us", ms / 1000, (ms % 1000) / 100);
        return buf;
    }

    std::string FormatFade(u32 ms) {
        if (ms == 0) {
            return "Off";
        }

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%u.%us", ms / 1000, (ms % 1000) / 100);
        return buf;
    }

    /* Nearest entry in the list, so a hand edited config still lands somewhere. */
    template<typename T>
    size_t FindNearestIndex(const T &values, u32 ms) {
        size_t best = 0;
        u32 best_diff = UINT32_MAX;

        for (size_t i = 0; i < values.size(); i++) {
            const u32 diff = values[i] > ms ? values[i] - ms : ms - values[i];
            if (diff < best_diff) {
                best_diff = diff;
                best = i;
            }
        }

        return best;
    }

    /* The configured start up item, empty when start up loading is off. */
    std::string GetSelectedTrack() {
        char path[FS_MAX_PATH];
        if (config::get_load_path(path, sizeof(path))) {
            return path;
        }

        /* Nothing set, so the default folder is in use. */
        return "";
    }

    /* Folder the track list is built from: wherever the start up item lives,
       falling back to the default music folder. */
    std::string GetMusicFolder() {
        char path[FS_MAX_PATH];
        if (config::get_load_path(path, sizeof(path))) {
            FsDirEntryType type;
            if (R_SUCCEEDED(sdmc::GetType(path, &type))) {
                if (type == FsDirEntryType_Dir) {
                    return path;
                }

                /* A file, so list its folder. */
                const std::string full{path};
                const auto slash = full.find_last_of('/');
                if (slash != std::string::npos && slash > 0) {
                    return full.substr(0, slash);
                }
            }
        }

        return config::DEFAULT_LOAD_PATH;
    }

    std::string FileNameOf(const std::string &path) {
        const auto slash = path.find_last_of('/');
        return slash == std::string::npos ? path : path.substr(slash + 1);
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
    bool restart_on_resume = false;
    bool startup_enabled = true;
    u32 wake_delay = 1500;
    u32 fade_ms = 400;
    float volume = 1.f;

    tuneGetHomeMenuOnly(&home_menu_only);
    tuneGetFocusDetect(&focus_detect);
    tuneGetFocusDetectAvailable(&focus_detect_available);
    tuneGetAutoPlay(&autoplay);
    tuneGetPauseOnSleep(&pause_on_sleep);
    tuneGetResumeOnWake(&resume_on_wake);
    tuneGetPauseOnHeadphoneUnplug(&pause_on_unplug);
    tuneGetRestartOnResume(&restart_on_resume);
    tuneGetStartupEnabled(&startup_enabled);
    tuneGetWakeDelayMs(&wake_delay);
    tuneGetFadeMs(&fade_ms);
    tuneGetVolume(&volume);

    list->addItem(new tsl::elm::CategoryHeader("Music"));

    /* Which track the home menu plays. */
    const auto selected = GetSelectedTrack();
    auto track_item = new tsl::elm::ListItem("Home menu track");
    track_item->setValue(selected.empty() ? "Folder" : FileNameOf(selected));
    track_item->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<TrackGui>();
            return true;
        }
        return false;
    });
    list->addItem(track_item);

    /* Volume, saved to the config by the sysmodule. */
    auto volume_slider = new ElmVolume("\uE13C", "Volume", num_steps);
    volume_slider->setProgress(volume * num_steps);
    volume_slider->setValueChangedListener([](u8 value) {
        tuneSetVolume(float(value) / float(num_steps));
    });
    list->addItem(volume_slider);

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

    /* Play the jingle once on the boot logo screen. */
    auto startup_toggle = new tsl::elm::ToggleListItem("Boot jingle", startup_enabled, "On", "Off");
    startup_toggle->setStateChangedListener([](bool new_value) {
        tuneSetStartupEnabled(new_value);
    });
    list->addItem(startup_toggle);

    /* How long the music takes to ease in and out. */
    auto fade_item = new tsl::elm::ListItem("Fade");
    fade_item->setValue(FormatFade(fade_ms));
    fade_item->setClickListener([fade_item, index = FindNearestIndex(FADE_LENGTHS, fade_ms)](u64 keys) mutable {
        if (keys & HidNpadButton_A) {
            index = (index + 1) % FADE_LENGTHS.size();
            tuneSetFadeMs(FADE_LENGTHS[index]);
            fade_item->setValue(FormatFade(FADE_LENGTHS[index]));
            return true;
        }
        return false;
    });
    list->addItem(fade_item);

    list->addItem(new tsl::elm::CategoryHeader("Resuming"));

    /* Start over rather than carrying on from the middle. */
    auto restart_toggle = new tsl::elm::ToggleListItem("Restart track", restart_on_resume, "On", "Off");
    restart_toggle->setStateChangedListener([](bool new_value) {
        tuneSetRestartOnResume(new_value);
    });
    list->addItem(restart_toggle);

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
    delay_item->setClickListener([delay_item, index = FindNearestIndex(WAKE_DELAYS, wake_delay)](u64 keys) mutable {
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

tsl::elm::Element *TrackGui::createUI() {
    auto frame = new SysTuneOverlayFrame();
    auto list  = new tsl::elm::List();

    const auto folder = GetMusicFolder();
    const auto selected = GetSelectedTrack();

    /* Collect the playable files in the folder. */
    std::vector<std::string> names;

    FsDir dir;
    if (R_SUCCEEDED(sdmc::OpenDir(&dir, folder.c_str(), FsDirOpenMode_ReadFiles | FsDirOpenMode_NoFileSize))) {
        std::vector<FsDirectoryEntry> entries(64);
        s64 count = 0;

        while (R_SUCCEEDED(fsDirRead(&dir, &count, entries.size(), entries.data())) && count) {
            for (s64 i = 0; i < count; i++) {
                if (SupportsType(entries[i].name)) {
                    names.emplace_back(entries[i].name);
                }
            }

            /* The playlist tops out at 300 entries, no point listing more. */
            if (names.size() >= 300) {
                break;
            }
        }

        fsDirClose(&dir);
    }

    std::sort(names.begin(), names.end(), [](const std::string &lhs, const std::string &rhs) {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    });

    list->addItem(new tsl::elm::CategoryHeader(folder));

    if (names.empty()) {
        list->addItem(new tsl::elm::CategoryHeader("No music found here"));
        frame->setContent(list);
        return frame;
    }

    /* Shared so every row can clear the others' mark when one is picked. */
    auto rows = std::make_shared<std::vector<tsl::elm::ListItem *>>();

    /* Play the whole folder rather than a single track. */
    auto folder_item = new tsl::elm::ListItem("Whole folder");
    folder_item->setClickListener([rows, folder, folder_item, frame](u64 keys) {
        if (keys & HidNpadButton_A) {
            /* The folder is only scanned at boot, so leave whatever is
               playing alone rather than cutting it off until then. */
            config::set_load_path(folder.c_str());

            for (auto row : *rows) {
                row->setValue("");
            }
            folder_item->setValue("Selected");

            frame->setToast("Playing whole folder", "Takes effect on the next reboot");
            return true;
        }
        return false;
    });
    rows->push_back(folder_item);
    list->addItem(folder_item);

    for (const auto &name : names) {
        const auto path = folder + "/" + name;

        auto item = new tsl::elm::ListItem(name);
        if (path == selected) {
            item->setValue("Selected");
        }

        item->setClickListener([rows, path, item, frame](u64 keys) {
            if (keys & HidNpadButton_A) {
                /* Make it the only track, and remember it for next boot. */
                tuneClearQueue();
                const Result rc = tuneEnqueue(path.c_str(), TuneEnqueueType_Back);
                if (R_FAILED(rc)) {
                    frame->setToast("Couldn't play that track", "Try a plain ASCII file name");
                    return true;
                }

                config::set_load_path(path.c_str());

                for (auto row : *rows) {
                    row->setValue("");
                }
                item->setValue("Selected");
                return true;
            }
            return false;
        });

        rows->push_back(item);
        list->addItem(item);
    }

    /* Mark the folder row when no single track is set. */
    if (selected.empty() || selected == folder) {
        folder_item->setValue("Selected");
    }

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
    bool startup_enabled = true;

    tuneGetHomeMenuOnly(&home_menu_only);
    tuneGetFocusDetect(&focus_detect);
    tuneGetFocusDetectAvailable(&focus_detect_available);
    tuneGetResumeOnWake(&resume_on_wake);
    tuneGetPauseOnHeadphoneUnplug(&pause_on_unplug);
    tuneGetStartupEnabled(&startup_enabled);

    /* Focus detection only does anything when pdm:qry is available. */
    const bool focus = focus_detect && focus_detect_available;

    /* Under "home menu only" a game is silent unless that title was turned */
    /* on by hand, which is the per game toggle on the main page. */
    const bool plays_in_games = !home_menu_only;

    list->addItem(new tsl::elm::CategoryHeader("With your current settings"));

    AddAnswer(list, "Boot logo screen", startup_enabled);
    AddAnswer(list, "Home menu", true);
    AddAnswer(list, "Settings, eShop, Album", true);
    AddAnswer(list, "Playing a game", plays_in_games);
    AddAnswer(list, "HOME over a game", plays_in_games || focus);
    AddAnswer(list, "Lock screen", resume_on_wake && (plays_in_games || focus));
    AddAnswer(list, "Console asleep", false);
    AddAnswer(list, "Headphones unplugged", !pause_on_unplug);

    list->addItem(new tsl::elm::CategoryHeader("Notes"));

    std::string notes =
        "Boot jingle: plays once per power on, never when the console wakes "
        "from sleep, because the sysmodule only starts at boot.\n \n"
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

    AddNotes(list, notes, 340);

    frame->setContent(list);

    return frame;
}
