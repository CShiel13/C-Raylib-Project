#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "incidents.h"

#define SETTINGS_HELP_LINE_COUNT 16
#define SETTINGS_HELP_VISIBLE_LINES 7
#define MAP_CALL_CREATION_DEFAULT 0

static int IncidentNumberKeyAt(int index)
{
    static const int keys[] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR};
    return index >= 0 && index < 4 ? keys[index] : -1;
}

static IncidentType IncidentTypeForNumberKey(int key)
{
    for (int index = 0; index < 4; index++)
        if (key == IncidentNumberKeyAt(index)) return (IncidentType)index;
    return (IncidentType)-1;
}

static IncidentType CycleNextCallType(IncidentType current)
{
    if (current < INCIDENT_FIRE || current > INCIDENT_MEDICAL) return INCIDENT_FIRE;
    return (IncidentType)((current + 1) % 4);
}

static Rectangle NextCallTypeSelectorBounds(void)
{
    return (Rectangle){300, 43, 186, 31};
}

static Rectangle SettingsHelpViewportBounds(void)
{
    return (Rectangle){462, 554, 676, 136};
}

static Rectangle SettingsRoutesToggleBounds(void)
{
    return (Rectangle){470, 245, 680, 48};
}

static Rectangle SettingsCivilianToggleBounds(void)
{
    return (Rectangle){470, 301, 680, 48};
}

static Rectangle SettingsCityConditionBounds(void)
{
    return (Rectangle){470, 357, 680, 48};
}

static Rectangle SettingsAutoIncidentsBounds(void)
{
    return (Rectangle){470, 413, 680, 48};
}

static Rectangle SettingsMapCallToggleBounds(void)
{
    return (Rectangle){470, 469, 680, 48};
}

static Rectangle ResponseViewButtonBounds(void)
{
    return (Rectangle){1012, 94, 178, 32};
}

static Rectangle MapLegendBounds(void)
{
    return (Rectangle){700, 99, 260, 24};
}

static int SettingsHelpScrollByWheel(int current, float wheelDelta)
{
    int maximum = SETTINGS_HELP_LINE_COUNT - SETTINGS_HELP_VISIBLE_LINES;
    if (wheelDelta < 0.0f) current++;
    else if (wheelDelta > 0.0f) current--;
    if (current < 0) return 0;
    if (current > maximum) return maximum;
    return current;
}

static int MapClickShouldCreateCall(int createCallOnMap, int incidentHit, int callHit)
{
    return createCallOnMap && incidentHit < 0 && callHit < 0;
}

static int ManualIncomingNoticeNeeded(int manualMode, int previousCallCount,
                                      int currentCallCount)
{
    return manualMode && currentCallCount > previousCallCount;
}

#endif
