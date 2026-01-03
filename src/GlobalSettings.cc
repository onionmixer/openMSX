#include "GlobalSettings.hh"

#include "GlobalCommandController.hh"
#include "SettingsConfig.hh"

namespace openmsx {

GlobalSettings::GlobalSettings(GlobalCommandController& commandController_)
	: commandController(commandController_)
	, pauseSetting(commandController, "pause",
	       "pauses the emulation", true, Setting::Save::NO)  // Start paused by default for debugging
	, powerSetting(commandController, "power",
	        "turn power on/off", false, Setting::Save::NO)
	, autoSaveSetting(commandController, "save_settings_on_exit",
	        "automatically save settings when openMSX exits", true)
	, umrCallBackSetting(commandController, "umr_callback",
		"Tcl proc to call when an UMR is detected", {})
	, invalidPsgDirectionsSetting(commandController,
		"invalid_psg_directions_callback",
		"Tcl proc called when the MSX program has set invalid PSG port directions",
		"default_invalid_psg_directions_callback")
	, invalidPpiModeSetting(commandController,
		"invalid_ppi_mode_callback",
		"Tcl proc called when the MSX program has set an invalid PPI mode",
		"default_invalid_ppi_mode_callback")
	, resampleSetting(commandController, "resampler", "Resample algorithm",
		ResampledSoundDevice::ResampleType::HQ,
		EnumSetting<ResampledSoundDevice::ResampleType>::Map{
			{"hq",   ResampledSoundDevice::ResampleType::HQ},
			{"blip", ResampledSoundDevice::ResampleType::BLIP}})
	, speedManager(commandController)
	, throttleManager(commandController)
	// Debug streaming settings (default: ON, can be turned OFF via TCL)
	, debugStreamCpuSetting(commandController, "debug_stream_cpu",
		"Stream CPU register/flag updates to port 65505", true, Setting::Save::YES)
	, debugStreamMemSetting(commandController, "debug_stream_mem",
		"Stream memory read/write events to port 65505", true, Setting::Save::YES)
	, debugStreamIOSetting(commandController, "debug_stream_io",
		"Stream I/O port access events to port 65505", true, Setting::Save::YES)
	, debugStreamSlotSetting(commandController, "debug_stream_slot",
		"Stream slot/bank change events to port 65505", true, Setting::Save::YES)
{
	getPowerSetting().attach(*this);
}

GlobalSettings::~GlobalSettings()
{
	getPowerSetting().detach(*this);
	commandController.getSettingsConfig().setSaveSettings(
		autoSaveSetting.getBoolean());
}

// Observer<Setting>
void GlobalSettings::update(const Setting& setting) noexcept
{
	if (&setting == &getPowerSetting()) { // either on or off
		// automatically unpause after a power off/on cycle
		// this solved a bug, but apart from that this behaviour also
		// makes more sense
		try {
			getPauseSetting().setBoolean(false);
		} catch(...) {
			// Ignore. E.g. can trigger when a Tcl trace on the
			// pause setting triggers errors in the Tcl script.
		}
	}
}

} // namespace openmsx
