#include "gui_main.hpp"

#include "elm_overlayframe.hpp"
#include "elm_volume.hpp"
#include "gui_browser.hpp"
#include "gui_playlist.hpp"
#include "gui_settings.hpp"
#include "pm/pm.hpp"
#include "config/config.hpp"

#include <string>

namespace {
    constexpr const size_t num_steps = 20;
}

MainGui::MainGui() {
    m_status_bar    = new StatusBar();
}

tsl::elm::Element *MainGui::createUI() {
    auto frame = new SysTuneOverlayFrame();
    auto list  = new tsl::elm::List();

    u64 pid{}, tid{};
    pm::getCurrentPidTid(&pid, &tid);

    /* Prefer the sysmodule's view of the current title. It is the one deciding
       playback, and it reports a game suspended behind the home menu as the
       home menu, which is what the settings below should apply to. */
    u64 sys_tid{};
    if (R_SUCCEEDED(tuneGetCurrentTitleId(&sys_tid)) && sys_tid) {
        tid = sys_tid;
    }

    /* Non-null for the home menu and the system applets we track. */
    const char *applet_name = pm::GetSystemAppletName(tid);

    /* Current track. */
    list->addItem(this->m_status_bar, tsl::style::ListItemDefaultHeight * 2);

    /* Playlist. */
    auto queue_button = new tsl::elm::ListItem("Playlist");
    queue_button->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<PlaylistGui>();
            return true;
        }
        return false;
    });
    list->addItem(queue_button);

    /* Browser. */
    auto browser_button = new tsl::elm::ListItem("Music browser");
    browser_button->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<BrowserGui>();
            return true;
        }
        return false;
    });
    list->addItem(browser_button);

    /* Volume indicator */
    list->addItem(new tsl::elm::CategoryHeader("Volume Control"));

    /* Get initial volume. */
    float tune_volume = 1.f;
    float title_volume = 1.f;
    float default_title_volume = 1.f;

    tuneGetVolume(&tune_volume);
    tuneGetTitleVolume(&title_volume);
    tuneGetDefaultTitleVolume(&default_title_volume);

    auto tune_volume_slider = new ElmVolume("\uE13C", "Tune Volume", num_steps);
    tune_volume_slider->setProgress(tune_volume * num_steps);
    tune_volume_slider->setValueChangedListener([](u8 value){
        const float volume = float(value) / float(num_steps);
        tuneSetVolume(volume);
    });
    list->addItem(tune_volume_slider);

    if (tid && pid) {
        /* The home menu has its own process too, so this doubles as a way to
           quieten the menu sound effects under the music. */
        const std::string volume_label = applet_name
            ? std::string(applet_name) + " Volume"
            : std::string("Game Volume");

        auto title_volume_slider = new ElmVolume("\uE13C", volume_label, num_steps);
        title_volume_slider->setProgress(title_volume * num_steps);
        title_volume_slider->setValueChangedListener([tid](u8 value){
            const float volume = float(value) / float(num_steps);
            tuneSetTitleVolume(volume);
            config::set_title_volume(tid, volume);
        });
        list->addItem(title_volume_slider);
    }

    auto default_title_volume_slider = new ElmVolume("\uE13C", "Game Volume (default)", num_steps);
    default_title_volume_slider->setProgress(default_title_volume * num_steps);
    default_title_volume_slider->setValueChangedListener([](u8 value){
        const float volume = float(value) / float(num_steps);
        tuneSetDefaultTitleVolume(volume);
    });
    list->addItem(default_title_volume_slider);

    list->addItem(new tsl::elm::CategoryHeader("Play / Pause"));

    /* Per title tune toggle. This overrides "Home menu only". */
    const std::string here_label = applet_name
        ? std::string("Music in ") + applet_name
        : std::string("Music in this game");

    auto tune_play = new tsl::elm::ToggleListItem(here_label, config::get_title_enabled(tid), "Play", "Pause");
    tune_play->setStateChangedListener([tid](bool new_value) {
        config::set_title_enabled(tid, new_value);
        if (new_value) {
            tunePlay();
        } else {
            tunePause();
        }
    });
    list->addItem(tune_play);

    /* Default title tune toggle. */
    auto tune_default_play = new tsl::elm::ToggleListItem("Music by default", config::get_title_enabled_default(), "Play", "Pause");
    tune_default_play->setStateChangedListener([](bool new_value) {
        config::set_title_enabled_default(new_value);
        if (new_value) {
            tunePlay();
        } else {
            tunePause();
        }
    });
    list->addItem(tune_default_play);

    list->addItem(new tsl::elm::CategoryHeader("Misc"));

    /* Home menu music behaviour. */
    auto settings_button = new tsl::elm::ListItem("Home menu music");
    settings_button->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tsl::changeTo<SettingsGui>();
            return true;
        }
        return false;
    });
    list->addItem(settings_button);

    auto startup_button = new tsl::elm::ListItem("Remove start up file");
    startup_button->setClickListener([frame](u64 keys) {
        if (keys & HidNpadButton_A) {
            char path[512];
            if (config::get_load_path(path, sizeof(path))) {
                config::set_load_path("");
                const auto* p = path;
                if (auto ext = std::strrchr(path, '/')) {
                    p = ext + 1;
                }

                frame->setToast("Removed start up file", p);
            } else if (!config::has_load_path()) {
                /* Nothing set, so the default music folder is being loaded.
                   Write an explicit empty value to turn that off as well. */
                config::set_load_path("");
                frame->setToast("Start up loading off", "The default /music folder is no longer loaded at boot");
            } else {
                frame->setToast("Nothing to remove", "No start up file, and the default folder is already off");
            }
            return true;
        }
        return false;
    });
    list->addItem(startup_button);

    auto exit_button = new tsl::elm::ListItem("Close sys-tune");
    exit_button->setClickListener([](u64 keys) {
        if (keys & HidNpadButton_A) {
            tuneQuit();
            tsl::goBack();
            return true;
        }
        return false;
    });
    list->addItem(exit_button);

    frame->setContent(list);

    return frame;
}

void MainGui::update() {
    static u8 tick = 0;
    /* Update status 4 times per second. */
    if ((tick % 15) == 0)
        this->m_status_bar->update();
    tick++;
}
