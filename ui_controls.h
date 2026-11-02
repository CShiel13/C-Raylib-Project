#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "incidents.h"

#define SETTINGS_HELP_TOPIC_COUNT 5
#define SETTINGS_HELP_LINES_PER_TOPIC 5
#define MAP_CALL_CREATION_DEFAULT 0
#define INCIDENT_QUEUE_VISIBLE_ROWS 8

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
    return (Rectangle){458, 552, 684, 140};
}

static Rectangle SettingsHelpTabBounds(int tab)
{
    return (Rectangle){460.0f + (float)tab * 137.0f, 556.0f, 132.0f, 26.0f};
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

static int SettingsHelpTopicByWheel(int current, float wheelDelta)
{
    if (wheelDelta < 0.0f) current++;
    else if (wheelDelta > 0.0f) current--;
    if (current < 0) return SETTINGS_HELP_TOPIC_COUNT - 1;
    if (current >= SETTINGS_HELP_TOPIC_COUNT) return 0;
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

static int IncidentQueueClampOffset(int offset, int orderCount)
{
    int maximum = orderCount - INCIDENT_QUEUE_VISIBLE_ROWS;
    if (maximum < 0) maximum = 0;
    if (offset < 0) return 0;
    return offset > maximum ? maximum : offset;
}

static int IncidentQueueScrollOffset(int offset, float wheelDelta, int orderCount)
{
    if (wheelDelta > 0.0f) offset--;
    else if (wheelDelta < 0.0f) offset++;
    return IncidentQueueClampOffset(offset, orderCount);
}

// Keep drawing and row hit testing on the same ordered call index.
static int IncidentQueueCallAtRow(const int *order, int orderCount,
                                  int offset, int row)
{
    if (!order || row < 0 || row >= INCIDENT_QUEUE_VISIBLE_ROWS) return -1;
    offset = IncidentQueueClampOffset(offset, orderCount);
    int index = offset + row;
    return index < orderCount ? order[index] : -1;
}

#endif
