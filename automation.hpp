
#ifndef __DSP_AUTOMATION__
#define __DSP_AUTOMATION__

#include <stdint.h>
#include <FS.h>

typedef enum {
  AUTOMATION_SET_ONOFF,
  AUTOMATION_SET_LIMITS
} automation_step_type_t;


typedef struct automation_ctx_t {
  uint8_t c[16];
} automation_ctx_t;


typedef struct automation_crondef_t {
  int8_t min;
  int8_t hour;
  int8_t day;
  int8_t month;
  int8_t dow;
} automation_crondef_t;


typedef struct automation_cronentry_t {
  automation_crondef_t crondef;
  automation_step_type_t step_type;
  automation_ctx_t context;
} automation_cronentry_t;

typedef std::vector<automation_cronentry_t> automation_t;

// Enable/disable automation
void automation_set_enabled(bool enabled);

// Get automation state
bool automation_get_enabled();

// Load automation settings
void automation_load(File source);

// Store automation settings
void automation_store(File dst);

automation_t automation_entries();

// Add a new entry to crontab
void automation_crontab_add(automation_cronentry_t entry);

// Remove line from crontab
void automation_crontab_remove(int n);

// Execute whatever automation is installed:
void automation_run();

// cron entry: onoff
automation_cronentry_t automation_entry_onoff(automation_crondef_t cd, bool state);

// cron entry: set
automation_cronentry_t automation_entry_limits(automation_crondef_t cd, uint16_t voltage, uint16_t current);

void automation_parse_onoff(automation_ctx_t* ctx, bool &state);

void automation_parse_limits(automation_ctx_t* ctx, uint16_t &voltage, uint16_t &current);

#endif

