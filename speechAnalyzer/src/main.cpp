#include <string>
#include <iostream>

#include <boost/program_options.hpp>

#include "GlobalMosquittoListener.h"
#include "arguments.h"
#include "ASRProcessor.h"

using namespace boost::program_options;
using namespace std;

Arguments JsonBuilder::args;
int main(int argc, char *argv[]){
	std::cout << "Starting speechAnalyzer, awaiting for trial to begin... " << std::endl;
	Arguments args;
    	try {
		options_description desc{"Options"};
		desc.add_options()("help,h", "Help screen")(
		    "mqtt_host",
		    value<string>(&args.mqtt_host)->default_value("mosquitto"),
		    "The host address of the mqtt broker")(
		    "mqtt_port",
		    value<int>(&args.mqtt_port)->default_value(1883),
		    "The port of the mqtt broker")(
		    "mqtt_host_internal",
		    value<string>(&args.mqtt_host_internal)->default_value("mosquitto_internal_speechAnalyzer"),
		    "The host of the internal mqtt server")(
		    "mqtt_port_internal",
		    value<int>(&args.mqtt_port_internal)->default_value(1883),
		    "The host of the internal mqtt server");


		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
    }
    catch(exception e){
	
    }
    JsonBuilder::args = args; 
    // Setup Global Listener
    GLOBAL_LISTENER.connect(args.mqtt_host, args.mqtt_port, 1000, 1000, 1000);
    GLOBAL_LISTENER.subscribe("trial");
    GLOBAL_LISTENER.subscribe("experiment");
    GLOBAL_LISTENER.set_max_seconds_without_messages(
        2147483647); // Max Long value
    GLOBAL_LISTENER_THREAD = thread([] { GLOBAL_LISTENER.loop(); });
    ASRProcessor *processor = new ASRProcessor(args.mqtt_host, args.mqtt_port, args.mqtt_host_internal, args.mqtt_port_internal);
    while(true){

    }
}
