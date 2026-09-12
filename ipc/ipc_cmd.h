#pragma once

enum TuneIpcCmd {
    TuneIpcCmd_GetStatus = 0,
    TuneIpcCmd_Play = 1,
    TuneIpcCmd_Pause = 2,
    TuneIpcCmd_Next = 3,
    TuneIpcCmd_Prev = 4,

    TuneIpcCmd_GetVolume = 10,
    TuneIpcCmd_SetVolume = 11,
    TuneIpcCmd_GetTitleVolume = 12,
    TuneIpcCmd_SetTitleVolume = 13,
    TuneIpcCmd_GetDefaultTitleVolume = 14,
    TuneIpcCmd_SetDefaultTitleVolume = 15,

    TuneIpcCmd_GetRepeatMode = 20,
    TuneIpcCmd_SetRepeatMode = 21,
    TuneIpcCmd_GetShuffleMode = 22,
    TuneIpcCmd_SetShuffleMode = 23,

    TuneIpcCmd_GetPlaylistSize = 30,
    TuneIpcCmd_GetPlaylistItem = 31,
    TuneIpcCmd_GetCurrentQueueItem = 32,
    TuneIpcCmd_ClearQueue = 33,
    TuneIpcCmd_MoveQueueItem = 34,
    TuneIpcCmd_Select = 35,
    TuneIpcCmd_Seek = 36,

    TuneIpcCmd_Enqueue = 40,
    TuneIpcCmd_Remove = 41,

    /* Home menu music behaviour. */
    TuneIpcCmd_GetHomeMenuOnly = 60,
    TuneIpcCmd_SetHomeMenuOnly = 61,
    TuneIpcCmd_GetFocusDetect = 62,
    TuneIpcCmd_SetFocusDetect = 63,
    TuneIpcCmd_GetAutoPlay = 64,
    TuneIpcCmd_SetAutoPlay = 65,
    TuneIpcCmd_GetPauseOnSleep = 66,
    TuneIpcCmd_SetPauseOnSleep = 67,
    TuneIpcCmd_GetResumeOnWake = 68,
    TuneIpcCmd_SetResumeOnWake = 69,
    TuneIpcCmd_GetWakeDelayMs = 70,
    TuneIpcCmd_SetWakeDelayMs = 71,
    TuneIpcCmd_GetPauseOnHeadphoneUnplug = 72,
    TuneIpcCmd_SetPauseOnHeadphoneUnplug = 73,
    TuneIpcCmd_GetCurrentTitleId = 74,
    TuneIpcCmd_GetFocusDetectAvailable = 75,
    TuneIpcCmd_GetRestartOnResume = 76,
    TuneIpcCmd_SetRestartOnResume = 77,
    TuneIpcCmd_GetStartupEnabled = 78,
    TuneIpcCmd_SetStartupEnabled = 79,
    TuneIpcCmd_GetFadeMs = 80,
    TuneIpcCmd_SetFadeMs = 81,

    TuneIpcCmd_QuitServer = 50,

    TuneIpcCmd_GetApiVersion = 5000,
};
