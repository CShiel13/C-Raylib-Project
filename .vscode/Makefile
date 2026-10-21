CC = clang
CFLAGS = -std=c99 -Wall -Wextra
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)
LDLIBS = $(RAYLIB_LIBS) -lm

.PHONY: all check-events check-traffic check-ui

all: main

main: main.c simulation.c simulation.h incidents.h ui_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) main.c simulation.c -o main $(LDLIBS)

feature_event_condition_check: feature_event_condition_check.c simulation.c simulation.h incidents.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) feature_event_condition_check.c simulation.c -o $@ $(LDLIBS)

dispatch_traffic_check: dispatch_traffic_check.c simulation.c simulation.h incidents.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) dispatch_traffic_check.c simulation.c -o $@ $(LDLIBS)

ui_click_check: ui_click_check.c ui_layout.h
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) ui_click_check.c -o $@ $(LDLIBS)

check-events: feature_event_condition_check
	./feature_event_condition_check

check-traffic: dispatch_traffic_check
	./dispatch_traffic_check

check-ui: ui_click_check
	./ui_click_check
