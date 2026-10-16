#ifndef INCIDENTS_H
#define INCIDENTS_H

#include "raylib.h"

#define MAX_INCIDENTS 100
#define MAX_VEHICLES 9
#define SERVICE_COUNT 3
#define MAX_ROUTE_POINTS 16
#define MAX_CIVILIAN_CARS 24

typedef enum {
    INCIDENT_FIRE = 0,
    INCIDENT_CRIME,
    INCIDENT_ACCIDENT,
    INCIDENT_MEDICAL
} IncidentType;

typedef enum {
    INCIDENT_PENDING = 0,
    INCIDENT_DISPATCHED,
    INCIDENT_RESPONDING,
    INCIDENT_RESOLVED,
    INCIDENT_FAILED
} IncidentStatus;

typedef enum {
    TRAFFIC_LIGHT = 0,
    TRAFFIC_MEDIUM,
    TRAFFIC_HEAVY,
    TRAFFIC_LEVEL_COUNT
} TrafficLevel;

typedef struct {
    IncidentType type;
    IncidentStatus status;
    Vector2 position;
    float ageTime;
    float waitTime;
    float travelTime;
    float responseTime;
    float workElapsed;
    float workRequired;
    float deadline;
    int severity;
    int requiredUnits;
    int responseRecorded;
} Incident;

typedef struct {
    IncidentType serviceType;
    Vector2 position;
    Vector2 stationPosition;
    float speed;
    int available;
    int assignedIncident;
    int arrived;
    Vector2 route[MAX_ROUTE_POINTS];
    int routeCount;
    int routeIndex;
} Vehicle;

typedef struct {
    Vector2 position;
    int fromNode;
    int toNode;
    float progress;
    float speed;             // Preferred free-flow speed, varied deterministically
    float currentSpeed;
    float yieldTime;
    float sirenReactionDelay;  // Stable per-driver response delay
    float sirenReactionTimer;  // Countdown while an emergency vehicle approaches
    int sirenSource;           // Emergency unit this driver is yielding to
    int routeCursor;
    int laneIndex;
    float headingDegrees;
} CivilianCar;

#endif
