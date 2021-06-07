#include <thread>
#include <mutex>
#include "TrialListener.h"
#include "GlobalMosquittoListener.h"

using namespace std;

TrialListener GLOBAL_LISTENER;
thread GLOBAL_LISTENER_THREAD;
mutex OPENSMILE_MUTEX;
