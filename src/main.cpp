// STDLIB
#include <string>
#include <fstring>

// Boost
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/chrono.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/program_options.hpp>

// Local
#include "arguments.h"
#include "Manager.h"

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

using namespace boost::program_options;
using namespace std;

int main(int argc, char* argv[]) {
    // Enable Boost logging
    boost::log::add_common_attributes();
    boost::log::add_console_log(std::cout,
                                boost::log::keywords::auto_flush = true,
                                boost::log::keywords::format =
				(
				    expr::stream
					<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
					<< ": <" << logging::trivial::severity
					<< "> " << expr::smessage
				));
    boost::log::add_file_log(boost::log::keywords::file_name = "/logs/%Y-%m-%d_%H-%M-%S.%N.log",
                             boost::log::keywords::auto_flush = true,
                             boost::log::keywords::format =
			     (
				    expr::stream
					<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
					<< ": <" << logging::trivial::severity
					<< "> " << expr::smessage
				));

    // Get SpeechAnalyzer version number
    ifstream file("conf/SpeechAnalyzer_version.txt");
    string version;
    file >> version;
    BOOST_LOG_TRIVIAL(info)
        << "SpeechAnalyzer version: " << version;

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
            value<string>(&args.mqtt_host_internal)
                ->default_value("mosquitto_internal_speechAnalyzer"),
            "The host of the internal mqtt server")(
            "mqtt_port_internal",
            value<int>(&args.mqtt_port_internal)->default_value(1883),
            "The host of the internal mqtt server");

        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
    }
    catch (exception e) {
    	BOOST_LOG_TRIVIAL(error)
        	<< "Unable to parse arguments, exiting... ";
	exit(-1);
    }
 
    BOOST_LOG_TRIVIAL(info)
        << "Starting speechAnalyzer, awaiting for trial to begin... ";
    
    // Launch Manager
    Manager manager(args.mqtt_host,
                    args.mqtt_port,
                    args.mqtt_host_internal,
                    args.mqtt_port_internal);

    // Yield main thread until exit
    chrono::milliseconds duration(1);
    while (true) {
        this_thread::yield();
        this_thread::sleep_for(duration);
    }
}
