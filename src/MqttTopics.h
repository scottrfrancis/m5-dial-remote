#pragma once

// Default MQTT topic names.
// Override any of these in environment.h by defining them before this header
// is included. For example:
//
//   #define TOPIC_FAN_STATE "fan/living_room/state"
//
// See environment.h.example for the full list.

// Water heater
#ifndef TOPIC_WATER_TEMP
#define TOPIC_WATER_TEMP    "water/temp"
#endif
#ifndef TOPIC_WATER_RECIRC
#define TOPIC_WATER_RECIRC  "water/recirc"
#endif
#ifndef TOPIC_WATER_CMD
#define TOPIC_WATER_CMD     "water/recirc/cmd"
#endif

// Ceiling fan
#ifndef TOPIC_FAN_STATE
#define TOPIC_FAN_STATE     "fan/bedroom/state"
#endif
#ifndef TOPIC_FAN_CMD
#define TOPIC_FAN_CMD       "fan/bedroom/command"
#endif

// Ceiling light
#ifndef TOPIC_LIGHT_STATE
#define TOPIC_LIGHT_STATE   "fan/bedroom/light"
#endif
#ifndef TOPIC_LIGHT_CMD
#define TOPIC_LIGHT_CMD     "light/bedroom/command"
#endif
