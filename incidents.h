#ifndef INCIDENTS_H
#define INCIDENTS_H

#include "raylib.h"

#define MAX_INCIDENTS 100
#define MAX_VEHICLES 8
#define SERVICE_COUNT 3

typedef enum {
    INCIDENT_FIRE = 0,
    INCIDENT_CRIME,
    INCIDENT_ACCIDENT
} IncidentType;

typedef enum {
    INCIDENT_PENDING = 0,
    INCIDENT_DISPATCHED,
    INCIDENT_RESPONDING,
    INCIDENT_RESOLVED
} IncidentStatus;

typedef struct {
    IncidentType type;
    IncidentStatus status;
    Vector2 position;
    float ageTime;          // Simulated time since incident was created
    float workElapsed;      // Simulated time spent resolving the incident
    float responseTime;     // Time from creation until a unit arrives
    float responseDuration;
    int assignedVehicle;
} Incident;

typedef struct {
    IncidentType serviceType;
    Vector2 position;
    Vector2 stationPosition;
    float speed;
    int available;
    int assignedIncident;
} Vehicle;

#endif
