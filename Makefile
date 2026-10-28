CC = clang
CFLAGS = -std=c99 -Wall -Wextra
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)
LDLIBS = $(RAYLIB_LIBS) -lm

.PHONY: all check-events check-traffic check-ui check-response check-response-view check-operator check-city-layout

all: main

main: main.c simulation.c simulation.h incidents.h city_layout.h street_color.h ui_layout.h manual_selection.h manual_dispatch.h response_selection.h response_camera.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) main.c simulation.c -o main $(LDLIBS)

feature_event_condition_check: feature_event_condition_check.c simulation.c simulation.h incidents.h city_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) feature_event_condition_check.c simulation.c -o $@ $(LDLIBS)

dispatch_traffic_check: dispatch_traffic_check.c simulation.c simulation.h incidents.h city_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) dispatch_traffic_check.c simulation.c -o $@ $(LDLIBS)

ui_click_check: ui_click_check.c ui_layout.h response_camera.h city_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) ui_click_check.c -o $@ $(LDLIBS)

response_stage_check: response_stage_check.c simulation.c simulation.h incidents.h city_layout.h manual_dispatch.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) response_stage_check.c simulation.c -o $@ $(LDLIBS)

operator_override_check: operator_override_check.c simulation.c simulation.h incidents.h city_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) operator_override_check.c simulation.c -o $@ $(LDLIBS)

check-events: feature_event_condition_check
	./feature_event_condition_check

check-traffic: dispatch_traffic_check
	./dispatch_traffic_check

check-ui: ui_click_check
	./ui_click_check

check-response: response_stage_check
	./response_stage_check

check-response-view: response_view_check.c response_selection.h response_camera.h city_layout.h simulation.h incidents.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) response_view_check.c -o /tmp/emergency_city_response_view_check $(LDLIBS)
	/tmp/emergency_city_response_view_check

city_layout_check: city_layout_check.c city_layout.h street_color.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) city_layout_check.c -o /tmp/emergency_city_city_layout_check -lm
	/tmp/emergency_city_city_layout_check

check-city-layout: city_layout_check

check-operator: operator_override_check
	./operator_override_check

living_city_check: living_city_check.c simulation.c simulation.h incidents.h city_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) living_city_check.c simulation.c -o $@ $(LDLIBS)

.PHONY: check-living-city
check-living-city: living_city_check
	./living_city_check
