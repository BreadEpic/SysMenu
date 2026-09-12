#pragma once

#include "../tune_types.hpp"
#include <string>
#include <vector>

namespace tune::impl {

    Result Initialize();
    void Exit();

    void TuneThreadFunc(void *);
    void GpioThreadFunc(void *);
    void PmdmntThreadFunc(void *);
    void PscmThreadFunc(void *);

    bool GetStatus();
    void Play();
    void Pause();
    void Next();
    void Prev();

    float GetVolume();
    void SetVolume(float volume);
    float GetTitleVolume();
    void SetTitleVolume(float volume);
    float GetDefaultTitleVolume();
    void SetDefaultTitleVolume(float volume);

    void TitlePlay();
    void TitlePause();
    void DefaultTitlePlay();
    void DefaultTitlePause();

    /* Home menu music behaviour. */
    bool GetHomeMenuOnly();
    void SetHomeMenuOnly(bool value);
    bool GetFocusDetect();
    void SetFocusDetect(bool value);
    /// False when pdm:qry could not be opened, focus detection then does nothing.
    bool GetFocusDetectAvailable();
    bool GetAutoPlay();
    void SetAutoPlay(bool value);
    bool GetPauseOnSleep();
    void SetPauseOnSleep(bool value);
    bool GetResumeOnWake();
    void SetResumeOnWake(bool value);
    u32 GetWakeDelayMs();
    void SetWakeDelayMs(u32 value);
    bool GetPauseOnHeadphoneUnplug();
    void SetPauseOnHeadphoneUnplug(bool value);
    u32 GetFadeMs();
    void SetFadeMs(u32 value);
    bool GetRestartOnResume();
    void SetRestartOnResume(bool value);
    bool GetStartupEnabled();
    void SetStartupEnabled(bool value);
    /// Title the sysmodule currently considers active, see pm::getCurrentPidTid.
    u64 GetCurrentTitleId();

    RepeatMode GetRepeatMode();
    void SetRepeatMode(RepeatMode mode);
    ShuffleMode GetShuffleMode();
    void SetShuffleMode(ShuffleMode mode);

    u32 GetPlaylistSize();
    u32 GetPlaylistItem(u32 index, char* buffer, size_t buffer_size);
    Result GetCurrentQueueItem(CurrentStats *out, char* buffer, size_t buffer_size);
    void ClearQueue();
    void MoveQueueItem(u32 src, u32 dst);
    void Select(u32 index);
    void Seek(u32 position);

    Result Enqueue(const char* buffer, size_t buffer_length, EnqueueType type);
    Result Remove(u32 index);

}
