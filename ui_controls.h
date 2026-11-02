#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "incidents.h"

#define SETTINGS_HELP_LINE_COUNT 16
#define SETTINGS_HELP_VISIBLE_LINES 7

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

static int SettingsHelpScrollByWheel(int current, float wheelDelta)
{
    int maximum = SETTINGS_HELP_LINE_COUNT - SETTINGS_HELP_VISIBLE_LINES;
    if (wheelDelta < 0.0f) current++;
    else if (wheelDelta > 0.0f) current--;
    if (current < 0) return 0;
    if (current > maximum) return maximum;
    return current;
}

#endif
