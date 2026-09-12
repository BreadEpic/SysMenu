#include "pm.hpp"

namespace pm {
namespace {

u64 CURRENT_TITLE_ID{};
u64 CURRENT_PROCESS_ID{};

PdmPlayStatistics CURRENT_PLAY_STATS{};
PdmAppletEvent CURRENT_PLAY_EVENT{};
u64 LOST_FOCUS_EXPIRE_NS{};

/// Set when pdm:qry could be opened. Focus detection is skipped without it.
bool PDM_AVAILABLE{};
bool FOCUS_DETECT{true};

/**
 * Applets that get their own entry, so that playback can be configured for them
 * separately from the HOME Menu.
 *
 * qlaunch is always running and so is never probed for, it is the fallback.
 */
constexpr SystemAppletEntry SYSTEM_APPLET_IDS[] = {
    { "Home menu", SystemAppletId_qlaunch, true },
    { "Eshop", SystemAppletId_LibAppletShop, false },
    { "Album", SystemAppletId_photoViewer, false },
    { "Error screen", SystemAppletId_error, false },
};

/**
 * Applets that an application can launch while staying the focused application.
 *
 * Launching one of these makes the application lose focus, which would
 * otherwise be reported as "back at the HOME Menu" and start the music. So
 * bringing up the keyboard in a game must not start HOME Menu music.
 */
constexpr u64 IGNORE_APPLET_IDS[] = {
    SystemAppletId_auth,
    SystemAppletId_cabinet,
    SystemAppletId_controller,
    SystemAppletId_netConnect,
    SystemAppletId_playerSelect,
    SystemAppletId_swkbd,
    SystemAppletId_miiEdit,
    SystemAppletId_LibAppletWeb,
    SystemAppletId_LibAppletOff,
};

/// Grace period before a focus loss counts, so that loading an NRO through
/// hbmenu (which briefly drops focus) doesn't flip playback back and forth.
constexpr u64 LOST_FOCUS_DELAY_NS{500'000'000};

void SetPidTidToQlaunch(u64* pid_out, u64* tid_out) {
    *tid_out = SystemAppletId_qlaunch;

    u64 pid{};
    if (R_SUCCEEDED(pmdmntGetProcessId(&pid, SystemAppletId_qlaunch))) {
        *pid_out = pid;
    }
}

/**
 * Returns true while the application still owns the screen.
 *
 * There is no service that reports whether an application is suspended behind
 * the HOME Menu; applet IPC could answer it but is not usable from a sysmodule.
 * The play log records an in_focus / out_of_focus event on every transition
 * though, so the most recent event for the running application is read back
 * instead.
 */
bool IsApplicationFocused(u64 tid) {
    PdmPlayStatistics stats;
    if (R_FAILED(pdmqryQueryPlayStatisticsByApplicationId(tid, true, &stats))) {
        /* No play log for this title, assume it is in front of us. */
        return true;
    }

    /* A new entry means the focus state just changed. */
    if (stats.program_id != CURRENT_PLAY_STATS.program_id || stats.last_entry_index != CURRENT_PLAY_STATS.last_entry_index) {
        CURRENT_PLAY_STATS = stats;

        s32 total;
        if (R_SUCCEEDED(pdmqryQueryAppletEvent(stats.last_entry_index, true, &CURRENT_PLAY_EVENT, 1, &total)) && total) {
            if (CURRENT_PLAY_EVENT.event_type != PdmAppletEventType_InFocus) {
                /* Focus lost to an applet the application itself opened. */
                for (auto id : IGNORE_APPLET_IDS) {
                    u64 temp_pid;
                    if (R_SUCCEEDED(pmdmntGetProcessId(&temp_pid, id))) {
                        CURRENT_PLAY_EVENT.event_type = PdmAppletEventType_InFocus;
                        return true;
                    }
                }

                LOST_FOCUS_EXPIRE_NS = armTicksToNs(armGetSystemTick()) + LOST_FOCUS_DELAY_NS;
            }
        }

        return true;
    }

    if (CURRENT_PLAY_EVENT.event_type == PdmAppletEventType_InFocus) {
        return true;
    }

    /* Focus was lost, but only honour it once the grace period has passed. */
    return armTicksToNs(armGetSystemTick()) < LOST_FOCUS_EXPIRE_NS;
}

}

auto Initialize() -> Result {
    Result rc;
    if (R_FAILED(rc = pmdmntInitialize())) {
        return rc;
    }

    if (R_FAILED(rc = pminfoInitialize())) {
        pmdmntExit();
        return rc;
    }

    /* Optional: without it a suspended game simply keeps counting as in-game. */
    PDM_AVAILABLE = R_SUCCEEDED(pdmqryInitialize());

    return 0;
}

void Exit() {
    if (PDM_AVAILABLE) {
        pdmqryExit();
        PDM_AVAILABLE = false;
    }

    pminfoExit();
    pmdmntExit();
}

// SOURCE: https://github.com/retronx-team/sys-clk/blob/570f1e5fe10b253eff0c8fda1bb893bb620af052/sysmodule/src/process_management.cpp#L37
void getCurrentPidTid(u64* pid_out, u64* tid_out) {
    *tid_out = CURRENT_TITLE_ID;
    *pid_out = CURRENT_PROCESS_ID;

    /* Check if one of the tracked system applets is on screen. */
    for (auto& e : GetSystemAppletList()) {
        u64 pid{};
        if (!e.always_running && R_SUCCEEDED(pmdmntGetProcessId(&pid, e.id))) {
            *pid_out = pid;
            *tid_out = e.id;
            return;
        }
    }

    Result rc{};
    if (R_SUCCEEDED(rc = pmdmntGetApplicationProcessId(pid_out))) {
        if (0x20f == pminfoGetProgramId(tid_out, *pid_out)) {
            SetPidTidToQlaunch(pid_out, tid_out);
        } else if (PDM_AVAILABLE && FOCUS_DETECT && !IsApplicationFocused(*tid_out)) {
            /* An application is loaded but suspended behind the HOME Menu. */
            SetPidTidToQlaunch(pid_out, tid_out);
        }
    } else if (rc == 0x20f) {
        /* No application running at all. */
        SetPidTidToQlaunch(pid_out, tid_out);
    } else {
        *tid_out = CURRENT_TITLE_ID;
        *pid_out = CURRENT_PROCESS_ID;
    }
}

auto PollCurrentPidTid(u64* pid_out, u64* tid_out) -> bool {
    getCurrentPidTid(pid_out, tid_out);

    if (*tid_out != CURRENT_TITLE_ID || *pid_out != CURRENT_PROCESS_ID) {
        CURRENT_TITLE_ID = *tid_out;
        CURRENT_PROCESS_ID = *pid_out;
        return true;
    }

    return false;
}

auto IsSystemApplet(u64 tid) -> bool {
    /* Every system applet lives in this range, applications never do. */
    return tid >= SystemAppletId_qlaunch && tid <= 0x01000000000010FF;
}

auto GetSystemAppletName(u64 tid) -> const char* {
    for (auto& e : GetSystemAppletList()) {
        if (e.id == tid) {
            return e.name;
        }
    }

    return nullptr;
}

auto GetSystemAppletList() -> std::span<const SystemAppletEntry> {
    return SYSTEM_APPLET_IDS;
}

auto IsFocusDetectAvailable() -> bool {
    return PDM_AVAILABLE;
}

auto GetFocusDetect() -> bool {
    return FOCUS_DETECT;
}

void SetFocusDetect(bool enable) {
    FOCUS_DETECT = enable;

    /* Drop stale focus state so the next poll re-reads it. */
    CURRENT_PLAY_STATS = {};
    CURRENT_PLAY_EVENT = {};
    LOST_FOCUS_EXPIRE_NS = 0;
}

}
