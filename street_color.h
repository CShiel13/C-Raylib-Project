#ifndef STREET_COLOR_H
#define STREET_COLOR_H

#include "incidents.h"

#include <math.h>

// Apply the condition tint once to a source color. Fog distance haze is a
// separate transform so callers can compose both without repeating tinting.
static inline Color StreetConditionColor(Color color, CityCondition condition)
{
    if (condition == CITY_NIGHT) {
        color.r = (unsigned char)((float)color.r * 0.45f);
        color.g = (unsigned char)((float)color.g * 0.52f);
        color.b = (unsigned char)((float)color.b * 0.66f);
    } else if (condition == CITY_RAIN) {
        color.r = (unsigned char)((float)color.r * 0.88f);
        color.g = (unsigned char)((float)color.g * 0.94f);
        color.b = (unsigned char)fminf(255.0f, (float)color.b * 1.08f);
    }
    return color;
}

static inline Color StreetFogDistanceHaze(Color color, float distance)
{
    float haze = fmaxf(0.0f, fminf(0.62f, (distance - 6.0f) / 30.0f));
    color.r = (unsigned char)((float)color.r * (1.0f - haze) + 148.0f * haze);
    color.g = (unsigned char)((float)color.g * (1.0f - haze) + 169.0f * haze);
    color.b = (unsigned char)((float)color.b * (1.0f - haze) + 178.0f * haze);
    return color;
}

static inline Color StreetDistanceColor(Color color, Vector3 point, Vector3 camera,
                                        CityCondition condition)
{
    color = StreetConditionColor(color, condition);
    if (condition != CITY_FOG) return color;
    float dx = point.x - camera.x;
    float dz = point.z - camera.z;
    return StreetFogDistanceHaze(color, sqrtf(dx * dx + dz * dz));
}

#endif
