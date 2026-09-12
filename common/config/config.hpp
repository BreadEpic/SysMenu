#pragma once

#include <switch.h>

namespace config {

// tune shuffle
auto get_shuffle() -> bool;
void set_shuffle(bool value);

// tune repeat
auto get_repeat() -> int;
void set_repeat(int value);

// tune volume
auto get_volume() -> float;
void set_volume(float value);

// per title tune enable
auto has_title_enabled(u64 tid) -> bool;
auto get_title_enabled(u64 tid) -> bool;
void set_title_enabled(u64 tid, bool value);

// default for tune for every title
auto get_title_enabled_default() -> bool;
void set_title_enabled_default(bool value);

// per title volume
auto has_title_volume(u64 tid) -> bool;
auto get_title_volume(u64 tid) -> float;
void set_title_volume(u64 tid, float value);

// default volume for every title
auto get_default_title_volume() -> float;
void set_default_title_volume(float value);

// returns the length of the string
auto get_load_path(char* out, int max_len) -> int;
void set_load_path(const char* path);
// false when the key was never written, which is when the default folder applies
auto has_load_path() -> bool;

/* -- home menu music -- */

// folder used at boot when no start up file/folder has been set
constexpr const char DEFAULT_LOAD_PATH[]{"/music"};

// only play on the home menu and system applets, pause in games
auto get_home_menu_only() -> bool;
void set_home_menu_only(bool value);

// report a game suspended behind the home menu as the home menu
auto get_focus_detect() -> bool;
void set_focus_detect(bool value);

// start playback on boot without having to press play
auto get_autoplay() -> bool;
void set_autoplay(bool value);

// pause before the console sleeps
auto get_pause_on_sleep() -> bool;
void set_pause_on_sleep(bool value);

// resume once the console wakes back up
auto get_resume_on_wake() -> bool;
void set_resume_on_wake(bool value);

// how long to wait after waking before resuming, in milliseconds
auto get_wake_delay_ms() -> int;
void set_wake_delay_ms(int value);

// pause when the headphones are unplugged
auto get_pause_on_headphone_unplug() -> bool;
void set_pause_on_headphone_unplug(bool value);

// how long the fade in and out takes, in milliseconds, 0 disables it
auto get_fade_ms() -> int;
void set_fade_ms(int value);

// restart the track from the start when coming back from sleep or a game
auto get_restart_on_resume() -> bool;
void set_restart_on_resume(bool value);

// play the startup jingle once at boot
auto get_startup_enabled() -> bool;
void set_startup_enabled(bool value);

// the jingle played once at boot, on the console's boot logo screen
constexpr const char DEFAULT_STARTUP_PATH[]{"/music/startup.mp3"};
auto get_startup_path(char* out, int max_len) -> int;
void set_startup_path(const char* path);

}
