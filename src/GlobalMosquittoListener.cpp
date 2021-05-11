#include <thread>
#include "Mosquitto.h"
#include "GlobalMosquittoListener.h"

using namespace std;

MosquittoListener GLOBAL_LISTENER;
thread GLOBAL_LISTENER_THREAD;
