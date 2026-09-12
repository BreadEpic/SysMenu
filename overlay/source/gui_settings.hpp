#pragma once

#include "tune.h"

#include <tesla.hpp>

/**
 * @brief Home menu music behaviour settings.
 */
class SettingsGui final : public tsl::Gui {
  public:
    tsl::elm::Element *createUI() final;
};

/**
 * @brief Picks the track the home menu plays, and remembers it across reboots.
 */
class TrackGui final : public tsl::Gui {
  public:
    tsl::elm::Element *createUI() final;
};

/**
 * @brief Plain answer to "does music play here?" for every situation,
 *        worked out from the settings that are actually set right now.
 */
class BehaviourGui final : public tsl::Gui {
  public:
    tsl::elm::Element *createUI() final;
};
