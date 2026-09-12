#include "music_player.hpp"

#include "../tune_result.hpp"
#include "../tune_service.hpp"
#include "sdmc/sdmc.hpp"
#include "pm/pm.hpp"
#include "aud_wrapper.h"
#include "config/config.hpp"
#include "source.hpp"
#include "resamplers/SDL_audioEX.h"

#include <cstring>
#include <nxExt.h>

namespace tune::impl {

    namespace {
        constexpr float VOLUME_MAX = 1.f;
        constexpr auto PLAYLIST_ENTRY_MAX = 300; // 75k
        constexpr auto PATH_SIZE_MAX = 256;

        struct PlaylistID {
            u32 id{UINT32_MAX};

            bool IsValid() const {
                return id != UINT32_MAX;
            }

            void Reset() {
                id = UINT32_MAX;
            }
        };

        class PlayList {
        public:
            void Init() {
                Clear();
                m_playlist.reserve(PLAYLIST_ENTRY_MAX);
                m_shuffle_playlist.reserve(PLAYLIST_ENTRY_MAX);
            }

            bool Add(const char* path, EnqueueType type) {
                u32 index;
                if (!FindNextFreeEntry(index)) {
                    return false;
                }

                if (!m_entries[index].Add(path)) {
                    return false;
                }

                if (type == EnqueueType::Front) {
                    m_playlist.emplace(m_playlist.cbegin(), index);
                } else {
                    m_playlist.emplace_back(index);
                }

                // add new entry id to shuffle_playlist_list
                const auto shuffle_playlist_size = m_shuffle_playlist.size() + 1;
                const auto shuffle_index = randomGet64() % shuffle_playlist_size;
                m_shuffle_playlist.emplace(m_shuffle_playlist.cbegin() + shuffle_index, index);

                return true;
            }

            bool Remove(u32 index, ShuffleMode shuffle) {
                const auto entry = Get(index, shuffle);
                R_UNLESS(entry.IsValid(), false);

                // remove entry.
                m_entries[entry.id].Remove();

                // remove from both playlists.
                if (shuffle == ShuffleMode::On) {
                    m_playlist.erase(m_playlist.begin() + GetIndexFromID(entry, ShuffleMode::Off));
                    m_shuffle_playlist.erase(m_shuffle_playlist.begin() + index);
                } else {
                    m_playlist.erase(m_playlist.begin() + index);
                    m_shuffle_playlist.erase(m_shuffle_playlist.begin() + GetIndexFromID(entry, ShuffleMode::On));
                }

                return true;
            }

            bool Swap(u32 src, u32 dst, ShuffleMode shuffle) {
                if (src >= Size() || dst >+ Size()) {
                    return false;
                }

                if (shuffle == ShuffleMode::On) {
                    std::swap(m_shuffle_playlist[src], m_shuffle_playlist[dst]);
                } else {
                    std::swap(m_playlist[src], m_playlist[dst]);
                }

                return true;
            }

            void Shuffle() {
                const auto size = m_shuffle_playlist.size();
                if (!size) {
                    return;
                }

                for (auto& e : m_shuffle_playlist) {
                    const auto index = randomGet64() % size;
                    std::swap(e, m_shuffle_playlist[index]);
                }
            }

            const char* GetPath(u32 index, ShuffleMode shuffle) const {
                return GetPath(Get(index, shuffle));
            }

            const char* GetPath(const PlaylistID& entry) const {
                R_UNLESS(entry.IsValid(), nullptr);

                return m_entries[entry.id].GetPath();
            }

            void Clear() {
                for (u32 i = 0; i < m_entries.size(); i++) {
                    m_entries[i].Remove();
                }

                m_playlist.clear();
                m_shuffle_playlist.clear();
            }

            u32 Size() const {
                return m_playlist.size();
            }

            PlaylistID Get(u32 index, ShuffleMode shuffle) const {
                if (index >= Size()) {
                    return {};
                }

                if (shuffle == ShuffleMode::On) {
                    return m_shuffle_playlist[index];
                } else {
                    return m_playlist[index];
                }
            }

            u32 GetIndexFromID(const PlaylistID& entry, ShuffleMode shuffle) const {
                if (!entry.IsValid()) {
                    return 0;
                }

                std::span list{m_playlist};
                if (shuffle == ShuffleMode::On) {
                    list = m_shuffle_playlist;
                }

                for (u32 i = 0; i < list.size(); i++) {
                    if (list[i].id == entry.id) {
                        return i;
                    }
                }

                return 0;
            }

        private:
            bool FindNextFreeEntry(u32& index) const {
                for (u32 i = 0; i < m_entries.size(); i++) {
                    if (m_entries[i].IsEmpty()) {
                        index = i;
                        return true;
                    }
                }

                return false;
            }

        private:
            struct PlayListNameEntry {
            public:
                // in most cases, the path will not exceed 256 bytes,
                // so this is a reasonable max rather than 0x301.
                bool Add(const char* path) {
                    if (!IsEmpty()) {
                        return false;
                    }

                    if (std::strlen(path) >= sizeof(m_path)) {
                        return false;
                    }

                    std::strcpy(m_path, path);
                    return true;
                }

                bool Remove() {
                    m_path[0] = '\0';
                    return true;
                }

                bool IsEmpty() const {
                    return m_path[0] == '\0';
                }

                const char* GetPath() const {
                    return m_path;
                }

            private:
                char m_path[PATH_SIZE_MAX]{};
            };

        private:
            std::vector<PlaylistID> m_playlist{};
            std::vector<PlaylistID> m_shuffle_playlist{};
            std::array<PlayListNameEntry, PLAYLIST_ENTRY_MAX> m_entries{};
        };

        PlayList g_playlist;

        PlaylistID g_current;
        u32 g_queue_position;

        LockableMutex g_mutex;

        RepeatMode g_repeat   = RepeatMode::All;
        ShuffleMode g_shuffle = ShuffleMode::Off;
        PlayerStatus g_status = PlayerStatus::FetchNext;
        Source *g_source = nullptr;

        float g_title_volume = 1.f;
        float g_default_title_volume = 1.f;
        bool g_use_title_volume = true;

        constexpr auto AUDIO_FREQ          = 48000;
        constexpr auto AUDIO_CHANNEL_COUNT = 2;
        constexpr auto AUDIO_BUFFER_COUNT  = 2;
        constexpr auto AUDIO_LATENCY_MS    = 42;
        constexpr auto AUDIO_BUFFER_SIZE   = AUDIO_FREQ / 1000 * AUDIO_LATENCY_MS * AUDIO_CHANNEL_COUNT;

        AudioOutBuffer g_audout_buffer[AUDIO_BUFFER_COUNT];
        alignas(0x1000) s16 AudioMemoryPool[AUDIO_BUFFER_COUNT][(AUDIO_BUFFER_SIZE + 0xFFF) & ~0xFFF];
        static_assert((sizeof(AudioMemoryPool[0]) % 0x2000) == 0, "Audio Memory pool needs to be page aligned!");

        bool g_should_pause      = false;
        bool g_should_run        = true;

        /* Home menu music behaviour, mirrored from the config. */
        bool g_home_menu_only    = true;
        bool g_autoplay          = true;
        bool g_pause_on_sleep    = true;
        bool g_resume_on_wake    = true;
        u32 g_wake_delay_ms      = 1500;
        bool g_pause_on_headphone_unplug = true;

        bool g_restart_on_resume = false;
        bool g_startup_enabled   = true;

        /* Output gain, ramped towards 0 or 1 so that pausing for a game and
           coming back don't cut in and out abruptly. */
        float g_fade_gain        = 0.f;
        u32 g_fade_ms            = 400;

        /* Ramps the gain across one buffer of interleaved s16 frames. */
        void ApplyFade(s16* samples, size_t size_bytes, bool fade_out) {
            const float target = fade_out ? 0.f : 1.f;

            /* Ordinary playback at full volume, leave the samples alone. */
            if (target == 1.f && g_fade_gain >= 1.f) {
                return;
            }

            const size_t count = size_bytes / sizeof(s16);
            if (!count) {
                return;
            }

            float end = target;
            if (g_fade_ms) {
                /* Fraction of the ramp this buffer covers. */
                const float buffer_ms = float(count / AUDIO_CHANNEL_COUNT) * 1000.f / float(AUDIO_FREQ);
                const float step = buffer_ms / float(g_fade_ms);
                end = fade_out ? std::max(target, g_fade_gain - step)
                               : std::min(target, g_fade_gain + step);
            }

            const float start = g_fade_gain;
            const float delta = end - start;

            for (size_t i = 0; i < count; i++) {
                const float gain = start + delta * (float(i) / float(count));
                samples[i] = static_cast<s16>(float(samples[i]) * gain);
            }

            g_fade_gain = end;
        }

        /* Set at boot when autoplay is off, cleared once the user presses play. */
        bool g_autoplay_blocked  = false;

        u64 g_current_tid        = 0;

        /* Start the current track over, used when coming back from sleep or a
           game if the user asked for that. */
        void RestartTrack() {
            /* Hand it back to the tune thread rather than seeking the decoder
               from here: the queue position is untouched, so it re-opens the
               same track and plays it from the top. */
            g_status = PlayerStatus::FetchNext;
        }

        /* Should music play for this title? */
        bool ShouldPlayForTitle(u64 tid) {
            /* An explicit per title choice always wins. */
            if (config::has_title_enabled(tid)) {
                return config::get_title_enabled(tid);
            }

            /* Home menu music: play on the HOME Menu and system applets only. */
            if (g_home_menu_only) {
                return pm::IsSystemApplet(tid);
            }

            return config::get_title_enabled_default();
        }

        void ApplyTitleState(u64 tid) {
            g_current_tid = tid;

            if (g_autoplay_blocked) {
                g_should_pause = true;
                return;
            }

            const bool was_paused = g_should_pause;
            g_should_pause = !ShouldPlayForTitle(tid);

            /* Closing a game hands the screen back: start the track over
               rather than resuming from the middle. */
            if (was_paused && !g_should_pause && g_restart_on_resume) {
                RestartTrack();
            }
        }

        /* Everything that isn't needed to get the first sample out. Each ini
           read is its own sd card hit, so this runs on the pmdmnt thread
           alongside the boot jingle rather than holding playback up. */
        void LoadSettings() {
            /* Fetch values from config, sanitize the return value */
            if (auto c = config::get_repeat(); c <= 2 && c >= 0) {
                SetRepeatMode(static_cast<RepeatMode>(c));
            }

            SetShuffleMode(static_cast<ShuffleMode>(config::get_shuffle()));

            /* Assigned rather than set: the setters write the value straight
               back out again, which is a wasted sd card write at boot. */
            g_default_title_volume = std::clamp(config::get_default_title_volume(), 0.f, VOLUME_MAX);

            g_home_menu_only = config::get_home_menu_only();
            g_pause_on_sleep = config::get_pause_on_sleep();
            g_resume_on_wake = config::get_resume_on_wake();
            g_wake_delay_ms = std::clamp(config::get_wake_delay_ms(), 0, 10000);
            g_pause_on_headphone_unplug = config::get_pause_on_headphone_unplug();
            g_restart_on_resume = config::get_restart_on_resume();
            g_fade_ms = std::clamp(config::get_fade_ms(), 0, 5000);
            pm::SetFocusDetect(config::get_focus_detect());
        }

        Result PlayTrack(const char* path, bool one_shot = false) {
            /* Open file and allocate */
            auto source = OpenFile(path);
            R_UNLESS(source != nullptr, tune::FileOpenFailure);
            R_UNLESS(source->IsOpen(), tune::FileOpenFailure);
            R_UNLESS(source->SetupResampler(audoutGetChannelCount(), audoutGetSampleRate()), tune::VoiceInitFailure);

            AudioOutState state;
            R_TRY(audoutGetAudioOutState(&state));
            if (state == AudioOutState_Stopped) {
                R_TRY(audoutStartAudioOut());
            }

            g_source = source.get();

            // for the first buffer, use very small buffer sizes to reduce latency between songs.
            int first = 1;

            // audout is running at this point, see above.
            bool paused = false;

            while (g_should_run && g_status == PlayerStatus::Playing) {
                /* Keep decoding whilst fading out, and only go quiet once the
                   ramp has actually reached silence. */
                const bool want_pause = g_should_pause;

                if (want_pause && g_fade_gain <= 0.f) {
                    /* Stop audout whilst paused so a game has the audio path
                       to itself rather than us feeding it silence. */
                    if (!paused) {
                        paused = true;
                        audoutStopAudioOut();
                    }
                    svcSleepThread(17'000'000);
                    continue;
                }

                if (paused) {
                    if (R_FAILED(audoutStartAudioOut())) {
                        /* Don't append to a stopped device: that surfaces as a */
                        /* playback error and would drop the track from the */
                        /* playlist. Stay paused and retry instead. */
                        svcSleepThread(17'000'000);
                        continue;
                    }
                    paused = false;
                }

                AudioOutBuffer* buffer = NULL;
                for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
                    bool has_buffer = false;
                    R_TRY(audoutContainsAudioOutBuffer(&g_audout_buffer[i], &has_buffer));
                    if (!has_buffer) {
                        buffer = &g_audout_buffer[i];
                        break;
                    }
                }

                if (!buffer) {
                    u32 released_count;
                    R_TRY(audoutWaitPlayFinish(&buffer, &released_count, UINT64_MAX));
                }

                bool error = false;
                if (buffer) {
                    auto buffer_size = AUDIO_BUFFER_SIZE * sizeof(s16);
                    if (first) {
                        first--;
                        buffer_size = std::min(512 * sizeof(s16), buffer_size);
                    }

                    const auto nSamples = source->Resample((u8*)buffer->buffer, buffer_size);
                    if (nSamples <= 0) {
                        error = true;
                    } else {
                        ApplyFade(static_cast<s16*>(buffer->buffer), nSamples, want_pause);
                        buffer->data_size = nSamples;
                        R_TRY(audoutAppendAudioOutBuffer(buffer));
                    }
                }

                if (error || source->Done()) {
                    /* A one shot track is not in the playlist, so advancing
                       would move the real queue on by one. */
                    if (!one_shot && g_repeat != RepeatMode::One) {
                        Next();
                    }
                    break;
                }
            }

            g_source = nullptr;

            return 0;
        }

    }

    Result Initialize() {
        for (int i = 0; i < AUDIO_BUFFER_COUNT; i++) {
            g_audout_buffer[i].buffer = AudioMemoryPool[i];
            g_audout_buffer[i].buffer_size = sizeof(AudioMemoryPool[i]);
        }

        R_TRY(audoutInitialize());

        /* Only what is needed to decide whether to make a sound, and how loud.
           The rest is deferred to LoadSettings(), see above. */
        audoutSetAudioOutVolume(std::clamp(config::get_volume(), 0.f, VOLUME_MAX));
        g_autoplay = config::get_autoplay();
        g_startup_enabled = config::get_startup_enabled();

        /* Stay quiet at boot until the user presses play. */
        g_autoplay_blocked = !g_autoplay;
        g_should_pause = g_autoplay_blocked;

        // reserves memory so that we don't allocate later on.
        g_playlist.Init();

        return 0;

    }

    void Exit() {
        g_should_run = false;
    }

    void TuneThreadFunc(void *) {
        /* One shot jingle for the console's boot logo screen. The sysmodule is
           only started at boot, so this runs once per power on and never when
           the console wakes from sleep. */
        if (g_startup_enabled && !g_autoplay_blocked) {
            char startup_path[PATH_SIZE_MAX];
            if (config::get_startup_path(startup_path, sizeof(startup_path))
                    && GetSourceType(startup_path) != SourceType::NONE
                    && sdmc::FileExists(startup_path)) {
                /* Full volume straight away: the jingle should be heard as
                   early as possible, not eased in. */
                g_fade_gain = 1.f;
                g_status = PlayerStatus::Playing;
                PlayTrack(startup_path, true);
                g_status = PlayerStatus::FetchNext;
            }
        }

        /* Whatever plays next eases in rather than cutting straight on. */
        g_fade_gain = 0.f;

        {
            char load_path[PATH_SIZE_MAX];
            if (!config::get_load_path(load_path, sizeof(load_path))) {
                /* Nothing has ever been configured, so fall back to the default */
                /* music folder and have home menu music work out of the box. An */
                /* empty value that was written on purpose means the user removed */
                /* their start up item, so honour that and load nothing. */
                if (config::has_load_path()) {
                    load_path[0] = '\0';
                } else {
                    std::snprintf(load_path, sizeof(load_path), "%s", config::DEFAULT_LOAD_PATH);
                }
            }

            {
                // check if the path is a file or folder.
                FsDirEntryType type;
                if (R_SUCCEEDED(sdmc::GetType(load_path, &type))) {
                    if (type == FsDirEntryType_File) {
                        // path is a file, load single entry.
                        if (GetSourceType(load_path) != SourceType::NONE) {
                            Enqueue(load_path, std::strlen(load_path), EnqueueType::Back);
                        }
                    } else {
                        // path is a folder, load all entries.
                        FsDir dir;
                        if (R_SUCCEEDED(sdmc::OpenDir(&dir, load_path, FsDirOpenMode_ReadFiles|FsDirOpenMode_NoFileSize))) {
                            // during init, we have a lot of memory to work with.
                            std::vector<FsDirectoryEntry> entries(std::min(64, PLAYLIST_ENTRY_MAX));

                            s64 total;
                            char full_path[PATH_SIZE_MAX];
                            Result rc = 0;

                            while (R_SUCCEEDED(fsDirRead(&dir, &total, entries.size(), entries.data())) && total) {
                                for (s64 i = 0; i < total; i++) {
                                    if (GetSourceType(entries[i].name) != SourceType::NONE) {
                                        std::snprintf(full_path, sizeof(full_path), "%s/%s", load_path, entries[i].name);
                                        rc = Enqueue(full_path, std::strlen(full_path), EnqueueType::Back);
                                        if (rc == tune::OutOfMemory) {
                                            break;
                                        }
                                    }
                                }

                                if (rc == tune::OutOfMemory) {
                                    break;
                                }
                            }

                            fsDirClose(&dir);
                        }
                    }
                }
            }
        }

        /* Run as long as we aren't stopped and no error has been encountered. */
        while (g_should_run) {
            g_current.Reset();
            {
                std::scoped_lock lk(g_mutex);

                const auto queue_size = g_playlist.Size();
                if (queue_size == 0) {
                    g_current.Reset();
                } else if (g_queue_position >= queue_size) {
                    g_queue_position = queue_size - 1;
                    continue;
                } else {
                    g_current = g_playlist.Get(g_queue_position, g_shuffle);
                }
            }

            /* Sleep if queue is empty. */
            if (!g_current.IsValid()) {
                svcSleepThread(100'000'000ul);
                continue;
            }

            g_status = PlayerStatus::Playing;
            /* Only play if playing and we have a track queued. */
            Result rc = PlayTrack(g_playlist.GetPath(g_current));

            /* Log error. */
            if (R_FAILED(rc)) {
                /* Remove track if something went wrong. */
                Remove(g_queue_position);
            }
        }

        audoutStopAudioOut();
        audoutExit();
    }

    void GpioThreadFunc(void *ptr) {
        GpioPadSession *session = static_cast<GpioPadSession *>(ptr);

        bool pre_unplug_pause = false;

        /* [0] Low == plugged in; [1] High == not plugged in. */
        GpioValue old_value = GpioValue_High;

        while (g_should_run) {
            /* Fetch current gpio value. */
            GpioValue value;
            if (R_SUCCEEDED(gpioPadGetValue(session, &value))) {
                if (g_pause_on_headphone_unplug) {
                    if (old_value == GpioValue_Low && value == GpioValue_High) {
                        pre_unplug_pause = g_should_pause;
                        g_should_pause     = true;
                    } else if (old_value == GpioValue_High && value == GpioValue_Low) {
                        if (!pre_unplug_pause)
                            g_should_pause = false;
                    }
                }
                old_value = value;
            }
            svcSleepThread(10'000'000);
        }
    }

    void PscmThreadFunc(void *ptr) {
        PscPmModule *module = static_cast<PscPmModule *>(ptr);

        /* Playback state from before the console went to sleep. */
        bool pre_sleep_pause = false;
        bool sleeping = false;
        bool wake_pending = false;

        while (g_should_run) {
            Result rc = eventWait(&module->event, 10'000'000);
            if (R_VALUE(rc) == KERNELRESULT(TimedOut))
                continue;
            if (R_VALUE(rc) == KERNELRESULT(Cancelled))
                break;

            PscPmState state;
            u32 flags;
            if (R_FAILED(pscPmModuleGetRequest(module, &state, &flags)))
                continue;

            switch (state) {
                /* NOTE: ReadySleep can be sent more than once, and the wake */
                /* event is occasionally missed, so only the first of each */
                /* transition is acted on. */
                case PscPmState_ReadySleep:
                    if (!sleeping) {
                        sleeping = true;
                        pre_sleep_pause = g_should_pause;
                    }
                    if (g_pause_on_sleep) {
                        g_should_pause = true;
                    }
                    break;

                case PscPmState_ReadyAwaken:
                    if (sleeping) {
                        sleeping = false;
                        wake_pending = true;
                    }
                    break;

                default:
                    break;
            }

            /* Acknowledge before doing anything slow; the power state */
            /* transition waits on every registered module. */
            pscPmModuleAcknowledge(module, state);

            if (wake_pending) {
                wake_pending = false;

                if (g_resume_on_wake) {
                    /* Give audio services a moment to come back up, */
                    /* otherwise the first samples crackle. */
                    if (g_wake_delay_ms) {
                        svcSleepThread(u64(g_wake_delay_ms) * 1'000'000ULL);
                    }

                    const bool was_paused = g_should_pause;
                    g_should_pause = pre_sleep_pause;

                    /* Unlocking after sleep starts the track over if asked. */
                    if (was_paused && !g_should_pause && g_restart_on_resume) {
                        RestartTrack();
                    }
                }
            }
        }
    }

    void PmdmntThreadFunc(void *) {
        LoadSettings();

        while (g_should_run) {
            u64 pid{}, new_tid{};
            if (pm::PollCurrentPidTid(&pid, &new_tid)) {
                g_title_volume = 1.f;

                if (config::has_title_volume(new_tid)) {
                    g_use_title_volume = true;
                    SetTitleVolume(std::clamp(config::get_title_volume(new_tid), 0.f, VOLUME_MAX));
                }

                // TODO(TJ): fade song in rather than abruptly playing to avoid jump scares
                ApplyTitleState(new_tid);
            }

            // sadly, we can't simply apply auda when the title changes
            // as it seems to apply to quickly, before the title opens audio
            // services, so the changes don't apply.
            // best option is to repeatdly set the out :/
            if (pid) {
                const auto v = g_use_title_volume ? g_title_volume : g_default_title_volume;
                audWrapperSetProcessMasterVolume(pid, 0, v);
                // audWrapperSetProcessRecordVolume(pid, 0, v);
            }

            svcSleepThread(10'000'000);
        }
    }

    bool GetStatus() {
        return !g_should_pause;
    }

    void Play() {
        /* An explicit play overrides "don't start at boot". */
        g_autoplay_blocked = false;
        g_should_pause = false;
    }

    void Pause() {
        g_should_pause = true;
    }

    void Next() {
        bool pause = false;
        {
            std::scoped_lock lk(g_mutex);

            if (g_queue_position < g_playlist.Size() - 1) {
                g_queue_position++;
            } else {
                g_queue_position = 0;
                if (g_repeat == RepeatMode::Off)
                    pause = true;
            }
        }
        g_status     = PlayerStatus::FetchNext;
        g_should_pause = pause;
    }

    void Prev() {
        {
            std::scoped_lock lk(g_mutex);

            if (g_queue_position > 0) {
                g_queue_position--;
            } else {
                g_queue_position = g_playlist.Size() - 1;
            }
        }
        g_status     = PlayerStatus::FetchNext;
        g_should_pause = false;
    }

    float GetVolume() {
        float volume = 1.F;
        audoutGetAudioOutVolume(&volume);
        return volume;
    }

    void SetVolume(float volume) {
        volume = std::clamp(volume, 0.f, VOLUME_MAX);
        audoutSetAudioOutVolume(volume);
        config::set_volume(volume);
    }

    float GetTitleVolume() {
        return g_title_volume;
    }

    void SetTitleVolume(float volume) {
        volume = std::clamp(volume, 0.f, VOLUME_MAX);
        g_title_volume = volume;
        g_use_title_volume = true;
    }

    float GetDefaultTitleVolume() {
        return g_default_title_volume;
    }

    void SetDefaultTitleVolume(float volume) {
        volume = std::clamp(volume, 0.f, VOLUME_MAX);
        g_default_title_volume = volume;
        config::set_default_title_volume(volume);
    }

    bool GetHomeMenuOnly() {
        return g_home_menu_only;
    }

    void SetHomeMenuOnly(bool value) {
        g_home_menu_only = value;
        config::set_home_menu_only(value);

        /* Apply straight away rather than on the next title change. */
        ApplyTitleState(g_current_tid);
    }

    bool GetFocusDetect() {
        return pm::GetFocusDetect();
    }

    void SetFocusDetect(bool value) {
        pm::SetFocusDetect(value);
        config::set_focus_detect(value);
    }

    bool GetFocusDetectAvailable() {
        return pm::IsFocusDetectAvailable();
    }

    bool GetAutoPlay() {
        return g_autoplay;
    }

    void SetAutoPlay(bool value) {
        g_autoplay = value;
        config::set_autoplay(value);
    }

    bool GetPauseOnSleep() {
        return g_pause_on_sleep;
    }

    void SetPauseOnSleep(bool value) {
        g_pause_on_sleep = value;
        config::set_pause_on_sleep(value);
    }

    bool GetResumeOnWake() {
        return g_resume_on_wake;
    }

    void SetResumeOnWake(bool value) {
        g_resume_on_wake = value;
        config::set_resume_on_wake(value);
    }

    u32 GetWakeDelayMs() {
        return g_wake_delay_ms;
    }

    void SetWakeDelayMs(u32 value) {
        g_wake_delay_ms = std::min(value, 10000u);
        config::set_wake_delay_ms(g_wake_delay_ms);
    }

    u32 GetFadeMs() {
        return g_fade_ms;
    }

    void SetFadeMs(u32 value) {
        g_fade_ms = std::min(value, 5000u);
        config::set_fade_ms(g_fade_ms);
    }

    bool GetRestartOnResume() {
        return g_restart_on_resume;
    }

    void SetRestartOnResume(bool value) {
        g_restart_on_resume = value;
        config::set_restart_on_resume(value);
    }

    bool GetStartupEnabled() {
        return g_startup_enabled;
    }

    void SetStartupEnabled(bool value) {
        g_startup_enabled = value;
        config::set_startup_enabled(value);
    }

    bool GetPauseOnHeadphoneUnplug() {
        return g_pause_on_headphone_unplug;
    }

    void SetPauseOnHeadphoneUnplug(bool value) {
        g_pause_on_headphone_unplug = value;
        config::set_pause_on_headphone_unplug(value);
    }

    u64 GetCurrentTitleId() {
        return g_current_tid;
    }

    RepeatMode GetRepeatMode() {
        return g_repeat;
    }

    void SetRepeatMode(RepeatMode mode) {
        g_repeat = mode;
    }

    ShuffleMode GetShuffleMode() {
        return g_shuffle;
    }

    void SetShuffleMode(ShuffleMode mode) {
        std::scoped_lock lk(g_mutex);

        // if we just enabled shuffle mode, re-shuffle the playlist.
        if (g_shuffle == ShuffleMode::Off && mode == ShuffleMode::On) {
            g_playlist.Shuffle();
        }

        g_shuffle = mode;
    }

    u32 GetPlaylistSize() {
        std::scoped_lock lk(g_mutex);

        return g_playlist.Size();
    }

    Result GetPlaylistItem(u32 index, char *buffer, size_t buffer_size) {
        std::scoped_lock lk(g_mutex);

        const auto path = g_playlist.GetPath(index, g_shuffle);
        R_UNLESS(path, tune::OutOfRange);

        std::snprintf(buffer, buffer_size, "%s", path);

        return 0;
    }

    Result GetCurrentQueueItem(CurrentStats *out, char *buffer, size_t buffer_size) {
        R_UNLESS(g_source != nullptr, tune::NotPlaying);
        R_UNLESS(g_source->IsOpen(), tune::NotPlaying);

        {
            std::scoped_lock lk(g_mutex);

            const auto path = g_playlist.GetPath(g_current);
            R_UNLESS(path, tune::NotPlaying);

            std::snprintf(buffer, buffer_size, "%s", path);
        }

        auto [current, total] = g_source->Tell();
        int sample_rate       = g_source->GetSampleRate();

        out->sample_rate   = sample_rate;
        out->current_frame = current;
        out->total_frames  = total;

        return 0;
    }

    void ClearQueue() {
        {
            std::scoped_lock lk(g_mutex);

            g_playlist.Clear();
            g_queue_position = 0;
        }
        g_status = PlayerStatus::FetchNext;
    }

    // currently unused (and untested).
    void MoveQueueItem(u32 src, u32 dst) {
        std::scoped_lock lk(g_mutex);

        if (!g_playlist.Swap(src, dst, g_shuffle)) {
            return;
        }

        if (g_queue_position == src) {
            g_queue_position = dst;
        }
    }

    void Select(u32 index) {
        {
            std::scoped_lock lk(g_mutex);

            const auto size = g_playlist.Size();
            if (!size) {
                return;
            }

            g_queue_position = std::min(index, size - 1);
        }
        g_status     = PlayerStatus::FetchNext;
        g_should_pause = false;
    }

    void Seek(u32 position) {
        if (g_source != nullptr && g_source->IsOpen())
            g_source->Seek(position);
    }

    Result Enqueue(const char *buffer, size_t buffer_length, EnqueueType type) {
        if (GetSourceType(buffer) == SourceType::NONE)
            return tune::InvalidPath;

        /* Ensure file exists. */
        if (!sdmc::FileExists(buffer))
            return tune::InvalidPath;

        std::scoped_lock lk(g_mutex);

        if (!g_playlist.Add(buffer, type)) {
            return tune::OutOfMemory;
        }

        // check if the current position still points to the same entry, update if not.
        if (g_current.IsValid() && g_current.id != g_playlist.Get(g_queue_position, g_shuffle).id) {
            g_queue_position = g_playlist.GetIndexFromID(g_current, g_shuffle);
            g_current = g_playlist.Get(g_queue_position, g_shuffle);
        }

        return 0;
    }

    Result Remove(u32 index) {
        std::scoped_lock lk(g_mutex);

        /* Ensure we don't operate out of bounds. */
        R_UNLESS(g_playlist.Size(), tune::QueueEmpty);

        if (!g_playlist.Remove(index, g_shuffle)) {
            return tune::OutOfRange;
        }

        /* Fetch a new track if we deleted the current song. */
        const bool fetch_new = g_queue_position == index;

        /* Lower current position if needed. */
        if (g_queue_position > index) {
            g_queue_position--;
        }

        if (fetch_new)
            g_status = PlayerStatus::FetchNext;

        return 0;
    }

}
