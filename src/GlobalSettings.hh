#ifndef GLOBALSETTINGS_HH
#define GLOBALSETTINGS_HH

#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "ResampledSoundDevice.hh"
#include "SpeedManager.hh"
#include "StringSetting.hh"
#include "ThrottleManager.hh"

#include "Observer.hh"

#include <memory>

namespace openmsx {

class GlobalCommandController;

/**
 * This class contains settings that are used by several other class
 * (including some singletons). This class was introduced to solve
 * lifetime management issues.
 */
class GlobalSettings final : private Observer<Setting>
{
public:
	explicit GlobalSettings(GlobalCommandController& commandController);
	~GlobalSettings();

	[[nodiscard]] BooleanSetting& getPauseSetting() {
		return pauseSetting;
	}
	[[nodiscard]] BooleanSetting& getPowerSetting() {
		return powerSetting;
	}
	[[nodiscard]] BooleanSetting& getAutoSaveSetting() {
		return autoSaveSetting;
	}
	[[nodiscard]] StringSetting& getUMRCallBackSetting() {
		return umrCallBackSetting;
	}
	[[nodiscard]] StringSetting& getInvalidPsgDirectionsSetting() {
		return invalidPsgDirectionsSetting;
	}
	[[nodiscard]] StringSetting& getInvalidPpiModeSetting() {
		return invalidPpiModeSetting;
	}
	[[nodiscard]] EnumSetting<ResampledSoundDevice::ResampleType>& getResampleSetting() {
		return resampleSetting;
	}
	[[nodiscard]] SpeedManager& getSpeedManager() {
		return speedManager;
	}
	[[nodiscard]] ThrottleManager& getThrottleManager() {
		return throttleManager;
	}

	// Debug streaming settings (default: ON, can be turned OFF via TCL)
	[[nodiscard]] BooleanSetting& getDebugStreamCpuSetting() {
		return debugStreamCpuSetting;
	}
	[[nodiscard]] BooleanSetting& getDebugStreamMemSetting() {
		return debugStreamMemSetting;
	}
	[[nodiscard]] BooleanSetting& getDebugStreamIOSetting() {
		return debugStreamIOSetting;
	}
	[[nodiscard]] BooleanSetting& getDebugStreamSlotSetting() {
		return debugStreamSlotSetting;
	}

private:
	// Observer<Setting>
	void update(const Setting& setting) noexcept override;

private:
	GlobalCommandController& commandController;

	BooleanSetting pauseSetting;
	BooleanSetting powerSetting;
	BooleanSetting autoSaveSetting;
	StringSetting  umrCallBackSetting;
	StringSetting  invalidPsgDirectionsSetting;
	StringSetting  invalidPpiModeSetting;
	EnumSetting<ResampledSoundDevice::ResampleType> resampleSetting;
	SpeedManager speedManager;
	ThrottleManager throttleManager;

	// Debug streaming settings (default: ON)
	BooleanSetting debugStreamCpuSetting;
	BooleanSetting debugStreamMemSetting;
	BooleanSetting debugStreamIOSetting;
	BooleanSetting debugStreamSlotSetting;
};

} // namespace openmsx

#endif
