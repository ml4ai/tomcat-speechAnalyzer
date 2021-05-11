#pragma once
#include <thread>
#include "Mosquitto.h"

extern MosquittoListener GLOBAL_LISTENER;
extern std::thread GLOBAL_LISTENER_THREAD;
