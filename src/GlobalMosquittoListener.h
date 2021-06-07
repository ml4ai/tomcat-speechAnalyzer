#pragma once
#include <mutex>
#include <thread>
#include "TrialListener.h"

extern TrialListener GLOBAL_LISTENER;
extern std::thread GLOBAL_LISTENER_THREAD;
extern std::mutex OPENSMILE_MUTEX;
