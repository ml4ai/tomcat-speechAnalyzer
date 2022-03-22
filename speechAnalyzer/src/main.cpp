#include <string>

#include <boost/program_options.hpp>

#include "arguments.h"
#include "ASRProcessor.h"

using namespace boost::program_options;
using namespace std;

int main(int argc, char *argv[]){
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
		    value<int>(&args.mqtt_port_internal)->default_value(1885),
		    "The host of the internal mqtt server");


		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
    }
    catch(exception e){
	
    }
    
    ASRProcessor *processor = new ASRProcessor(args);
    while(true){

    }
}
