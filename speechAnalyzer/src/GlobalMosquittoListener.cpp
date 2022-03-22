#include "GlobalMosquittoListener.h"
#include "TrialListener.h"
#include <mutex>
#include <thread>

using namespace std;

TrialListener GLOBAL_LISTENER;
thread GLOBAL_LISTENER_THREAD;
mutex OPENSMILE_MUTEX;
