#pragma once
#include "TrialListener.h"
#include <mutex>
#include <thread>

extern TrialListener GLOBAL_LISTENER;
extern std::thread GLOBAL_LISTENER_THREAD;
extern std::mutex OPENSMILE_MUTEX;
