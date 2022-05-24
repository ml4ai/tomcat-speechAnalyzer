// STDLIB
#include <cstdlib>
#include <string>
#include <thread>
#include <iostream>

// Third Party - Boost
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/trivial.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

// Third Party
#include <nlohmann/json.hpp>

// Local
#include "Mosquitto.h"
#include "DBWrapper.h"
#include "ASRProcessor.h"

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using namespace std;

ASRProcessor::ASRProcessor(string mqtt_host, int mqtt_port) {
    this->mqtt_host = mqtt_host;
    this->mqtt_port = mqtt_port;
    
    postgres = make_unique<DBWrapper>(10); // 10 Connections for ASR processing  

    mosquitto_client.connect(
	mqtt_host, mqtt_port, 1000, 1000, 1000);
}

void ASRProcessor::ProcessASRMessage(nlohmann::json m) {
    
    nlohmann::json message = m;
    string trial_id = m["msg"]["trial_id"];
    string experiment_id = m["msg"]["experiment_id"];

    // Generate aligned features
    vector<nlohmann::json> word_messages;
    for (nlohmann::json word_message : m["data"]["features"]["word_messages"]) {
        double start_time = word_message["start_time"];
        double end_time = word_message["end_time"];

        vector<nlohmann::json> history =
            postgres->FeaturesBetween(start_time,
                                            end_time,
                                            m["data"]["participant_id"],
                                            m["msg"]["trial_id"]);
        nlohmann::json features_output;
        if (history.size() == 0) {
            features_output = nullptr;
            word_message["features"] = nullptr;
        }
        else {
            for (auto& it : history[0].items()) {
                features_output[it.key()] = vector<double>();
            }
            // Load the features output from the history entries
            for (auto entry : history) {
                for (auto& it : history[0].items()) {
                    features_output[it.key()].push_back(entry[it.key()]);
                }
            }
            word_message["features"] = features_output.dump();
        }
        word_messages.push_back(word_message);
    }
    message["data"]["features"]["word_messages"] = word_messages;

    // Send to MMC Server
    message["data"]["word_messages"] =
        message["data"]["features"]["word_messages"];
    string features = message.dump();
    string mmc = ProcessMMCMessage(message.dump());
    try {
        message["data"]["sentiment"] = nlohmann::json::parse(mmc);
    }
    catch (std::exception e) {
        std::cout << "Unable to process response from mmc server" << std::endl;
        return;
    }


    // Format and publish sentiment message
    nlohmann::json sentiment;
    sentiment["header"] = create_common_header("observation", trial_id, experiment_id);
    sentiment["msg"] = create_common_msg("speech_analyzer:sentiment", trial_id, experiment_id);
    sentiment["data"]["utterance_id"] = message["data"]["utterance_id"];
    sentiment["data"]["sentiment"]["emotions"] =
        message["data"]["sentiment"]["emotions"];
    sentiment["data"]["sentiment"]["penultimate_emotions"] =
        message["data"]["sentiment"]["penultimate_emotions"];
    this->mosquitto_client.publish("agent/speech_analyzer/sentiment",
                                   sentiment.dump());
    BOOST_LOG_TRIVIAL(info) << sentiment.dump();

    // Format and publish personality message
    nlohmann::json personality;
    personality["header"] = create_common_header("observation", trial_id, experiment_id);
    personality["msg"] = create_common_msg("speech_analyzer:personality", trial_id, experiment_id);
    personality["data"]["utterance_id"] = message["data"]["utterance_id"];
    personality["data"]["personality"]["traits"] =
        message["data"]["sentiment"]["traits"];
    personality["data"]["personality"]["penultimate_traits"] =
        message["data"]["sentiment"]["penultimate_traits"];
    this->mosquitto_client.publish("agent/speech_analyzer/personality",
                                   personality.dump());
    BOOST_LOG_TRIVIAL(info) << personality.dump();
}

// Data for handling word/feature alignment messages
string ASRProcessor::ProcessMMCMessage(string message) {
    try {
        string host = "mmc";
        string port = "8001";
        string target = "/encode";
        int version = 11;

        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver(ioc);
        beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        // Set up an HTTP GET request message
        http::request<http::string_body> req{http::verb::get, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.body() = message;
        req.prepare_payload();

        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);

        // Write the message to standard out

        // Gracefully close the socket
        beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes
        // so don't bother reporting it.
        //
        if (ec && ec != beast::errc::not_connected)
            throw beast::system_error{ec};

        return res.body().data();
    }
    catch (std::exception const& e) {
        BOOST_LOG_TRIVIAL(error) << e.what();
    }
    return "";
}

// Methods for creating common message types
nlohmann::json ASRProcessor::create_common_header(string message_type, string trial_id, string experiment_id) {
    nlohmann::json header;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    header["timestamp"] = timestamp;
    header["message_type"] = message_type;
    header["version"] = "1.1";

    return header;
}

nlohmann::json ASRProcessor::create_common_msg(std::string sub_type, string trial_id, string experiment_id) {
    nlohmann::json message;
    string timestamp =
        boost::posix_time::to_iso_extended_string(
            boost::posix_time::microsec_clock::universal_time()) +
        "Z";

    message["timestamp"] = timestamp;
    message["experiment_id"] = experiment_id;
    message["trial_id"] = trial_id;
    message["version"] = "0.6";
    message["source"] = "tomcat_speech_analyzer";
    message["sub_type"] = sub_type;

    return message;
}
