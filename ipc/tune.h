#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <switch.h>

typedef enum {
    TuneShuffleMode_Off,
    TuneShuffleMode_On,

    TuneShuffleMode_Count,
} TuneShuffleMode;

typedef enum {
    TuneRepeatMode_Off,
    TuneRepeatMode_One,
    TuneRepeatMode_All,

    TuneRepeatMode_Count,
} TuneRepeatMode;

typedef enum {
    TuneEnqueueType_Front,
    TuneEnqueueType_Back,

    TuneEnqueueType_Count,
} TuneEnqueueType;

typedef struct {
    u32 sample_rate;
    u32 current_frame;
    u32 total_frames;
} TuneCurrentStats;

Result tuneInitialize();

void tuneExit();

/**
 * @brief Get the current status of playback.
 * @param[out] status \ref AudioOutState
 */
Result tuneGetStatus(bool *status);

Result tunePlay();
Result tunePause();
Result tuneNext();
Result tunePrev();

/**
 * @brief Get the current playback volume.
 * @note On FW lower than [6.0.0] this will set the decode volume.
 * @param[out] out volume value (linear factor).
 */
Result tuneGetVolume(float *out);

/**
 * @brief Set the playback volume.
 * @note On FW lower than [6.0.0] this will return the decode volume.
 * @param[in] volume volume value (linear factor).
 */
Result tuneSetVolume(float volume);

/**
 * @brief Get the volume of the current title
 * @param[out] out volume value (linear factor).
 */
Result tuneGetTitleVolume(float *out);

/**
 * @brief Set the volume of the current title
 * @param[in] volume volume value (linear factor).
 */
Result tuneSetTitleVolume(float volume);

/**
 * @brief Get the default volume of all titles
 * @param[out] out volume value (linear factor).
 */
Result tuneGetDefaultTitleVolume(float *out);

/**
 * @brief Set the default volume of all titles
 * @param[in] volume volume value (linear factor).
 */
Result tuneSetDefaultTitleVolume(float volume);

/**
 * @brief Get the current loop status.
 * @param[out] state \ref TuneRepeatMode
 */
Result tuneGetRepeatMode(TuneRepeatMode *state);

/**
 * @brief Set repeat mode.
 * @param[in] state \ref TuneRepeatMode
 */
Result tuneSetRepeatMode(TuneRepeatMode state);

Result tuneGetShuffleMode(TuneShuffleMode *state);
Result tuneSetShuffleMode(TuneShuffleMode state);

/**
 * @brief Get the current queue size.
 * @param[out] count remaining tracks after current.
 */
Result tuneGetPlaylistSize(u32 *count);

/**
 * @brief Read queue.
 * @param[out] read Amount written to buffer.
 * @param[out] out_path Path array FS_MAX_PATH * n
 * @param[in] out_path_length Size of the supplied path array.
 */
Result tuneGetPlaylistItem(u32 index, char *out_path, size_t out_path_length);

/**
 * @brief Get current song.
 * @param[out] out_path Path to current playing song.
 * @param[in] out_path_length Size of the out_path buffer. Path of the current track needs to fit.
 * @param[out] out \ref MusicCurrentTune
 */
Result tuneGetCurrentQueueItem(char *out_path, size_t out_path_length, TuneCurrentStats *out);

/**
 * @brief Clear queue.
 */
Result tuneClearQueue();
Result tuneMoveQueueItem(u32 src, u32 dst);
Result tuneSelect(u32 index);
Result tuneSeek(u32 position);

/**
 * @brief Add track to queue.
 * @note Must not include leading mount name.
 * @note Must match ^(sdmc:/.*.mp3)$
 * @param[in] path Path to file on sdcard.
 */
Result tuneEnqueue(const char *path, TuneEnqueueType type);

Result tuneRemove(u32 index);

/**
 * @brief Play only on the HOME Menu and system applets, pausing in games.
 */
Result tuneGetHomeMenuOnly(bool *out);
Result tuneSetHomeMenuOnly(bool value);

/**
 * @brief Treat a game suspended behind the HOME Menu as the HOME Menu.
 * @note Requires pdm:qry, otherwise this has no effect.
 */
Result tuneGetFocusDetect(bool *out);
Result tuneSetFocusDetect(bool value);

/**
 * @brief Whether pdm:qry could be opened; focus detection does nothing without it.
 */
Result tuneGetFocusDetectAvailable(bool *out);

/**
 * @brief Start playing at boot without having to press play.
 */
Result tuneGetAutoPlay(bool *out);
Result tuneSetAutoPlay(bool value);

/**
 * @brief Pause before the console goes to sleep.
 */
Result tuneGetPauseOnSleep(bool *out);
Result tuneSetPauseOnSleep(bool value);

/**
 * @brief Resume once the console wakes up, on the lock screen.
 */
Result tuneGetResumeOnWake(bool *out);
Result tuneSetResumeOnWake(bool value);

/**
 * @brief Delay in milliseconds between waking up and resuming playback.
 */
Result tuneGetWakeDelayMs(u32 *out);
Result tuneSetWakeDelayMs(u32 value);

/**
 * @brief Pause when the headphones are unplugged.
 */
Result tuneGetPauseOnHeadphoneUnplug(bool *out);
Result tuneSetPauseOnHeadphoneUnplug(bool value);

/**
 * @brief How long the fade in and out takes, in milliseconds. 0 disables it.
 */
Result tuneGetFadeMs(u32 *out);
Result tuneSetFadeMs(u32 value);

/**
 * @brief Start the track over when returning from sleep or from a game,
 *        instead of carrying on from where it was.
 */
Result tuneGetRestartOnResume(bool *out);
Result tuneSetRestartOnResume(bool value);

/**
 * @brief Play the startup jingle once on the console's boot logo screen.
 * @note Only ever on a cold boot; the sysmodule is not restarted on wake.
 */
Result tuneGetStartupEnabled(bool *out);
Result tuneSetStartupEnabled(bool value);

/**
 * @brief Title the sysmodule currently considers active.
 * @note This is the HOME Menu id whenever a game is suspended behind it.
 */
Result tuneGetCurrentTitleId(u64 *out);

Result tuneQuit();

Result tuneGetApiVersion(u32 *version);

#ifdef __cplusplus
}
#endif
