#include <AP_HAL.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_F4BY
#include "RCInput.h"
#include <drivers/drv_hrt.h>
#include <uORB/uORB.h>

using namespace F4BY;

extern const AP_HAL::HAL& hal;

void F4BYRCInput::init(void* unused)
{
	_perf_rcin = perf_alloc(PC_ELAPSED, "APM_rcin");
	_rc_sub = orb_subscribe(ORB_ID(input_rc));
	if (_rc_sub == -1) {
		hal.scheduler->panic("Unable to subscribe to input_rc");		
	}
	clear_overrides();
        pthread_mutex_init(&rcin_mutex, NULL);
}

uint8_t F4BYRCInput::valid_channels() 
{
    pthread_mutex_lock(&rcin_mutex);
    bool valid = _rcin.timestamp != _last_read || _override_valid;
    pthread_mutex_unlock(&rcin_mutex);
    return valid;
}

uint16_t F4BYRCInput::read(uint8_t ch) 
{
	if (ch >= RC_INPUT_MAX_CHANNELS) {
		return 0;
	}
        pthread_mutex_lock(&rcin_mutex);
	_last_read = _rcin.timestamp;
	_override_valid = false;
	if (_override[ch]) {
            uint16_t v = _override[ch];
            pthread_mutex_unlock(&rcin_mutex);
            return v;
	}
	if (ch >= _rcin.channel_count) {
            pthread_mutex_unlock(&rcin_mutex);
            return 0;
	}
	uint16_t v = _rcin.values[ch];
        pthread_mutex_unlock(&rcin_mutex);
        return v;
}

uint8_t F4BYRCInput::read(uint16_t* periods, uint8_t len) 
{
	if (len > RC_INPUT_MAX_CHANNELS) {
		len = RC_INPUT_MAX_CHANNELS;
	}
	for (uint8_t i = 0; i < len; i++){
		periods[i] = read(i);
	}
	return len;
}

bool F4BYRCInput::set_overrides(int16_t *overrides, uint8_t len) 
{
	bool res = false;
	for (uint8_t i = 0; i < len; i++) {
		res |= set_override(i, overrides[i]);
	}
	return res;
}

bool F4BYRCInput::set_override(uint8_t channel, int16_t override) {
	if (override < 0) {
		return false; /* -1: no change. */
	}
	if (channel >= RC_INPUT_MAX_CHANNELS) {
		return false;
	}
	_override[channel] = override;
	if (override != 0) {
		_override_valid = true;
		return true;
	}
	return false;
}

void F4BYRCInput::clear_overrides()
{
	for (uint8_t i = 0; i < RC_INPUT_MAX_CHANNELS; i++) {
		set_override(i, 0);
	}
}

void F4BYRCInput::_timer_tick(void)
{
	perf_begin(_perf_rcin);
	bool rc_updated = false;
	if (orb_check(_rc_sub, &rc_updated) == 0 && rc_updated) {
            pthread_mutex_lock(&rcin_mutex);
            orb_copy(ORB_ID(input_rc), _rc_sub, &_rcin);
            pthread_mutex_unlock(&rcin_mutex);
	}
        // note, we rely on the vehicle code checking valid_channels() 
        // and a timeout for the last valid input to handle failsafe
	perf_end(_perf_rcin);
}

#endif
