#pragma once

#include <switch.h>
#include <span>

namespace pm {

/**
 * System applet program ids.
 *
 * Verified against the switchbrew title list:
 * https://switchbrew.org/wiki/Title_list
 *
 * These are all "system applets" rather than applications, so none of them are
 * ever reported by pmdmntGetApplicationProcessId(). They have to be looked up
 * by program id with pmdmntGetProcessId() instead.
 */
enum SystemAppletId : u64 {
    SystemAppletId_qlaunch = 0x0100000000001000,       ///< HOME Menu (always running).
    SystemAppletId_auth = 0x0100000000001001,
    SystemAppletId_cabinet = 0x0100000000001002,       ///< amiibo.
    SystemAppletId_controller = 0x0100000000001003,
    SystemAppletId_dataErase = 0x0100000000001004,
    SystemAppletId_error = 0x0100000000001005,
    SystemAppletId_netConnect = 0x0100000000001006,
    SystemAppletId_playerSelect = 0x0100000000001007,
    SystemAppletId_swkbd = 0x0100000000001008,
    SystemAppletId_miiEdit = 0x0100000000001009,
    SystemAppletId_LibAppletWeb = 0x010000000000100A,
    SystemAppletId_LibAppletShop = 0x010000000000100B, ///< eShop.
    SystemAppletId_overlayDisp = 0x010000000000100C,
    SystemAppletId_photoViewer = 0x010000000000100D,   ///< Album.
    SystemAppletId_set = 0x010000000000100E,           ///< System Settings.
    SystemAppletId_LibAppletOff = 0x010000000000100F,  ///< Offline web (in-game manuals).
    SystemAppletId_LibAppletLns = 0x0100000000001010,
    SystemAppletId_LibAppletAuth = 0x0100000000001011,
    SystemAppletId_starter = 0x0100000000001012,
    SystemAppletId_myPage = 0x0100000000001013,
    SystemAppletId_maintenance = 0x0100000000001015,
    SystemAppletId_splay = 0x0100000000001048,
};

struct SystemAppletEntry {
    const char* name;
    u64 id;
    /// qlaunch is always running, so polling for it would always match.
    /// It is used as the fallback id instead of being probed for.
    bool always_running;
};

auto Initialize() -> Result;
void Exit();
void getCurrentPidTid(u64* pid_out, u64* tid_out);
auto PollCurrentPidTid(u64* pid_out, u64* tid_out) -> bool;

/// Returns true if the id belongs to the HOME Menu or one of the system applets.
auto IsSystemApplet(u64 tid) -> bool;

/// Human readable name for an id handled by this module, else nullptr.
auto GetSystemAppletName(u64 tid) -> const char*;

auto GetSystemAppletList() -> std::span<const SystemAppletEntry>;

/**
 * Focus detection reports a suspended application (HOME pressed during a game,
 * or the console waking on the lock screen) as the HOME Menu.
 *
 * It needs pdm:qry, which may not be available, see IsFocusDetectAvailable().
 */
auto IsFocusDetectAvailable() -> bool;
auto GetFocusDetect() -> bool;
void SetFocusDetect(bool enable);

}
