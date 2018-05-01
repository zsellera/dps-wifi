
#include <memory>
#include <vector>
#include <TimeLib.h>
#include <SD.h>
#include "automation.hpp"
#include "dps.hpp"


static bool m_enabled = false;
static std::vector<automation_cronentry_t> m_entries;

typedef struct onoff_t {
  bool state;
} onoff_t;

typedef struct limits_t {
  uint16_t voltage;
  uint16_t current;
} limits_t;

void handle_onoff(automation_cronentry_t e);
void handle_limits(automation_cronentry_t e);


// Enable/disable automation
void automation_set_enabled(bool enabled)
{
  m_enabled = enabled;
}


// Load automation settings
void automation_load(File source)
{
  automation_cronentry_t buff;

  m_entries.clear();
  while (1)
  {
    int n = source.read(&buff, sizeof(buff));
    if (n == sizeof(buff))
    {
      m_entries.push_back(buff);
    } else {
      return;
    }
  }
}


// Store automation settings
void automation_store(File dst)
{
  for (int i=0; i<m_entries.size(); ++i)
  {
    automation_cronentry_t e = m_entries[i];
    dst.write((const uint8_t *) &e, sizeof(automation_cronentry_t));
  }
}


// Add a new entry to crontab
void automation_crontab_add(automation_cronentry_t entry)
{
  m_entries.push_back(entry);
}


// Remove line from crontab
void automation_crontab_remove(int n)
{
  if (n >= 0 && n < m_entries.size())
  {
    m_entries.erase( m_entries.begin() + n);
  }
}


bool cron_matches(time_t t, automation_cronentry_t e)
{
  automation_crondef_t cd = e.crondef;
  
  if (cd.min > 0 && cd.min != minute(t))
  {
    return false;
  }

  if (cd.hour > 0 && cd.hour != hour(t))
  {
    return false;
  }

  if (cd.day > 0 && cd.day != day(t))
  {
    return false;
  }

  if (cd.month > 0 && cd.month != month(t))
  {
    return false;
  }

  if (cd.dow > 0 && cd.dow != weekday(t))
  {
    return false;
  }

  return true;
}

// Execute whatever automation is installed:
void automation_run()
{
  if (m_enabled)
  {
    time_t t = now();

    for (int i=0; i<m_entries.size(); ++i)
    {
      automation_cronentry_t entry = m_entries[i];
      if (cron_matches(t, entry))
      {
        switch (entry.step_type)
        {
          case AUTOMATION_SET_ONOFF:
          handle_onoff(entry);
          break;

          case AUTOMATION_SET_LIMITS:
          handle_limits(entry);
          break;

          default:
          break;
        }
      }
    }
  }
}

// cron entry: onoff
automation_cronentry_t automation_entry_onoff(automation_crondef_t cd, bool state)
{
  automation_cronentry_t e;
  onoff_t s = { state };
  
  memset(&e, 0, sizeof(automation_cronentry_t));
  e.crondef = cd;
  e.step_type = AUTOMATION_SET_ONOFF;
  memcpy(&e.context, &s, sizeof(s));

  return e;
}

// cron entry: set
automation_cronentry_t automation_entry_limits(automation_crondef_t cd, uint16_t voltage, uint16_t current)
{
  automation_cronentry_t e;
  limits_t s = { voltage, current };
  
  memset(&e, 0, sizeof(automation_cronentry_t));
  e.crondef = cd;
  e.step_type = AUTOMATION_SET_LIMITS;
  memcpy(&e.context, &s, sizeof(s));

  return e;
}


void handle_onoff(automation_cronentry_t e)
{
  onoff_t * s = (onoff_t *) &e.context;
  dps_set_onoff(s->state);
}

void handle_limits(automation_cronentry_t e)
{
  limits_t * s = (limits_t *) &e.context;
  dps_set_voltage_current(s->voltage, s->current);
  dps_set_onoff(true);
}

