#include "config.hpp"
#include "sdmc/sdmc.hpp"
#include "minIni/minIni.h"
#include <cstdio>

namespace config {

namespace {

const char CONFIG_PATH[]{"/config/sys-tune/config.ini"};

void create_config_dir() {
    /* Creating directory on every set call looks sus, but the user may delete the dir */
    /* whilst the sys-mod is running and then any changes made via the overlay */
    /* is lost, which sucks. */
    sdmc::CreateFolder("/config");
    sdmc::CreateFolder("/config/sys-tune");
}

auto get_tid_str(u64 tid) -> const char* {
    static char buf[21]{};
    std::sprintf(buf, "%016lX", tid);
    return buf;
}

}

auto get_shuffle() -> bool {
    return ini_getbool("config", "shuffle", false, CONFIG_PATH);
}

void set_shuffle(bool value) {
    create_config_dir();
    ini_putl("config", "shuffle", value, CONFIG_PATH);
}

auto get_repeat() -> int {
    return ini_getl("config", "repeat", 1, CONFIG_PATH);
}

void set_repeat(int value) {
    create_config_dir();
    ini_putl("config", "repeat", value, CONFIG_PATH);
}

auto get_volume() -> float {
    return ini_getf("config", "volume", 1.f, CONFIG_PATH);
}

void set_volume(float value) {
    create_config_dir();
    ini_putf("config", "volume", value, CONFIG_PATH);
}

auto has_title_enabled(u64 tid) -> bool {
    return ini_haskey("title", get_tid_str(tid), CONFIG_PATH);
}

auto get_title_enabled(u64 tid) -> bool {
    return ini_getbool("title", get_tid_str(tid), true, CONFIG_PATH);
}

void set_title_enabled(u64 tid, bool value) {
    create_config_dir();
    ini_putl("title", get_tid_str(tid), value, CONFIG_PATH);
}

auto get_title_enabled_default() -> bool {
    return ini_getbool("title", "default", true, CONFIG_PATH);
}

void set_title_enabled_default(bool value) {
    create_config_dir();
    ini_putl("title", "default", value, CONFIG_PATH);
}

auto has_title_volume(u64 tid) -> bool {
    return ini_haskey("volume", get_tid_str(tid), CONFIG_PATH);
}

auto get_title_volume(u64 tid) -> float {
    return ini_getf("volume", get_tid_str(tid), 1.f, CONFIG_PATH);
}

void set_title_volume(u64 tid, float value) {
    create_config_dir();
    ini_putf("volume", get_tid_str(tid), value, CONFIG_PATH);
}

auto get_default_title_volume() -> float {
    return ini_getf("config", "global_volume", 1.f, CONFIG_PATH);
}

void set_default_title_volume(float value) {
    create_config_dir();
    ini_putf("config", "global_volume", value, CONFIG_PATH);
}

auto get_load_path(char* out, int max_len) -> int {
    return ini_gets("config", "load_path", "", out, max_len, CONFIG_PATH);
}

void set_load_path(const char* path) {
    create_config_dir();
    ini_puts("config", "load_path", path, CONFIG_PATH);
}

auto has_load_path() -> bool {
    return ini_haskey("config", "load_path", CONFIG_PATH);
}

auto get_home_menu_only() -> bool {
    return ini_getbool("config", "home_menu_only", true, CONFIG_PATH);
}

void set_home_menu_only(bool value) {
    create_config_dir();
    ini_putl("config", "home_menu_only", value, CONFIG_PATH);
}

auto get_focus_detect() -> bool {
    return ini_getbool("config", "focus_detect", true, CONFIG_PATH);
}

void set_focus_detect(bool value) {
    create_config_dir();
    ini_putl("config", "focus_detect", value, CONFIG_PATH);
}

auto get_autoplay() -> bool {
    return ini_getbool("config", "autoplay", true, CONFIG_PATH);
}

void set_autoplay(bool value) {
    create_config_dir();
    ini_putl("config", "autoplay", value, CONFIG_PATH);
}

auto get_pause_on_sleep() -> bool {
    return ini_getbool("config", "pause_on_sleep", true, CONFIG_PATH);
}

void set_pause_on_sleep(bool value) {
    create_config_dir();
    ini_putl("config", "pause_on_sleep", value, CONFIG_PATH);
}

auto get_resume_on_wake() -> bool {
    return ini_getbool("config", "resume_on_wake", true, CONFIG_PATH);
}

void set_resume_on_wake(bool value) {
    create_config_dir();
    ini_putl("config", "resume_on_wake", value, CONFIG_PATH);
}

auto get_wake_delay_ms() -> int {
    return ini_getl("config", "wake_delay_ms", 1500, CONFIG_PATH);
}

void set_wake_delay_ms(int value) {
    create_config_dir();
    ini_putl("config", "wake_delay_ms", value, CONFIG_PATH);
}

auto get_pause_on_headphone_unplug() -> bool {
    return ini_getbool("config", "pause_on_headphone_unplug", true, CONFIG_PATH);
}

void set_pause_on_headphone_unplug(bool value) {
    create_config_dir();
    ini_putl("config", "pause_on_headphone_unplug", value, CONFIG_PATH);
}

auto get_fade_ms() -> int {
    return ini_getl("config", "fade_ms", 400, CONFIG_PATH);
}

void set_fade_ms(int value) {
    create_config_dir();
    ini_putl("config", "fade_ms", value, CONFIG_PATH);
}

auto get_restart_on_resume() -> bool {
    return ini_getbool("config", "restart_on_resume", false, CONFIG_PATH);
}

void set_restart_on_resume(bool value) {
    create_config_dir();
    ini_putl("config", "restart_on_resume", value, CONFIG_PATH);
}

auto get_startup_enabled() -> bool {
    return ini_getbool("config", "startup_enabled", true, CONFIG_PATH);
}

void set_startup_enabled(bool value) {
    create_config_dir();
    ini_putl("config", "startup_enabled", value, CONFIG_PATH);
}

auto get_startup_path(char* out, int max_len) -> int {
    return ini_gets("config", "startup_path", DEFAULT_STARTUP_PATH, out, max_len, CONFIG_PATH);
}

void set_startup_path(const char* path) {
    create_config_dir();
    ini_puts("config", "startup_path", path, CONFIG_PATH);
}

}
