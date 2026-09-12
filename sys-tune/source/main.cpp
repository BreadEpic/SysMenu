#include "impl/music_player.hpp"
#include "sdmc/sdmc.hpp"
#include "pm/pm.hpp"
#include "impl/aud_wrapper.h"
#include "impl/source.hpp"
#include "tune_service.hpp"
#include "tune_result.hpp"

namespace {
    /* Set when psc:m could be opened, see __appInit. */
    bool g_psc_service = false;
    bool g_psc_module = false;
}

extern "C" {
u32 __nx_applet_type     = AppletType_None;
u32 __nx_fs_num_sessions = 1;

// TODO(TJ): calculate minimum heap
// TODO(TJ): calculate reasonable amount of heap for playlist entries.
void __libnx_initheap(void) {
    static char inner_heap[1024 * 250];
    extern char *fake_heap_start;
    extern char *fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

void __appInit() {
    R_ABORT_UNLESS(smInitialize());
    R_ABORT_UNLESS(setsysInitialize());
    {
        SetSysFirmwareVersion version;
        R_ABORT_UNLESS(setsysGetFirmwareVersion(&version));
        hosversionSet(MAKEHOSVERSION(version.major, version.minor, version.micro));
        setsysExit();
    }

    R_ABORT_UNLESS(gpioInitialize());
    /* Optional: without it we simply don't react to sleep and wake up. */
    g_psc_service = R_SUCCEEDED(pscmInitialize());
    R_ABORT_UNLESS(fsInitialize());
    R_ABORT_UNLESS(audWrapperInitialize());
    R_ABORT_UNLESS(pm::Initialize());
    R_ABORT_UNLESS(sdmc::Open());
}

void __appExit(void) {
    sdmc::Close();
    pm::Exit();
    audWrapperExit();
    fsExit();
    if (g_psc_service) {
        pscmExit();
    }
    gpioExit();
    smExit();
}

} // extern "C"

namespace {

    alignas(0x1000) u8 gpioThreadBuffer[0x1000];
    alignas(0x1000) u8 pscmThreadBuffer[0x1000];
    alignas(0x1000) u8 pmdmntThreadBuffer[0x1000];
    alignas(0x1000) u8 tuneThreadBuffer[0x6000];

}

int main(int, char *[]) {
    R_ABORT_UNLESS(tune::impl::Initialize());

    /* Register audio as our dependency so we can pause before it prepares for sleep. */
    constexpr const u32 dependencies[] = { PscPmModuleId_Audio };

    /* Get pm module to listen for state change. Failing here only costs us */
    /* sleep handling, so it must not take the whole sysmodule down. */
    PscPmModule pm_module;
    if (g_psc_service) {
        g_psc_module = R_SUCCEEDED(pscmGetPmModule(&pm_module, PscPmModuleId(420), dependencies, sizeof(dependencies) / sizeof(u32), true));
    }

    /* Get GPIO session for the headphone jack pad. */
    GpioPadSession headphone_detect_session;
    R_ABORT_UNLESS(gpioOpenSession(&headphone_detect_session, GpioPadName(0x15)));

    ::Thread gpioThread;
    ::Thread pscmThread;
    ::Thread pmdmtThread;
    ::Thread tuneThread;
    R_ABORT_UNLESS(threadCreate(&gpioThread, tune::impl::GpioThreadFunc, &headphone_detect_session, gpioThreadBuffer, sizeof(gpioThreadBuffer), 0x20, -2));
    if (g_psc_module) {
        R_ABORT_UNLESS(threadCreate(&pscmThread, tune::impl::PscmThreadFunc, &pm_module, pscmThreadBuffer, sizeof(pscmThreadBuffer), 0x20, -2));
    }
    R_ABORT_UNLESS(threadCreate(&pmdmtThread, tune::impl::PmdmntThreadFunc, nullptr, pmdmntThreadBuffer, sizeof(pmdmntThreadBuffer), 0x20, -2));
    R_ABORT_UNLESS(threadCreate(&tuneThread, tune::impl::TuneThreadFunc, nullptr, tuneThreadBuffer, sizeof(tuneThreadBuffer), 0x20, -2));

    R_ABORT_UNLESS(threadStart(&gpioThread));
    if (g_psc_module) {
        R_ABORT_UNLESS(threadStart(&pscmThread));
    }
    R_ABORT_UNLESS(threadStart(&pmdmtThread));
    R_ABORT_UNLESS(threadStart(&tuneThread));

    /* Create services */
    R_ABORT_UNLESS(tune::InitializeServer());
    tune::LoopProcess();
    R_ABORT_UNLESS(tune::ExitServer());

    tune::impl::Exit();
    svcCancelSynchronization(gpioThread.handle);
    if (g_psc_module) {
        svcCancelSynchronization(pscmThread.handle);
    }

    R_ABORT_UNLESS(threadWaitForExit(&gpioThread));
    R_ABORT_UNLESS(threadWaitForExit(&pmdmtThread));
    R_ABORT_UNLESS(threadWaitForExit(&tuneThread));

    R_ABORT_UNLESS(threadClose(&gpioThread));
    R_ABORT_UNLESS(threadClose(&pmdmtThread));
    R_ABORT_UNLESS(threadClose(&tuneThread));

    if (g_psc_module) {
        R_ABORT_UNLESS(threadWaitForExit(&pscmThread));
        R_ABORT_UNLESS(threadClose(&pscmThread));
    }

    /* Close gpio session. */
    gpioPadClose(&headphone_detect_session);

    /* Unregister Psc module. */
    if (g_psc_module) {
        pscPmModuleFinalize(&pm_module);
        pscPmModuleClose(&pm_module);
    }

    return 0;
}
