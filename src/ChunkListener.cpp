#include "ChunkListener.h"

#include <iostream>
#include <string>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>
#include <nlohmann/json.hpp>

#include "base64.h"

using namespace std;

ChunkListener::ChunkListener(
    string participant_id,
    boost::lockfree::spsc_queue<vector<char>, boost::lockfree::capacity<1024>>*
        queue) {
    this->participant_id = participant_id;
    this->queue = queue;
}

void ChunkListener::on_message(const string& topic, const string& message) {
    nlohmann::json m = nlohmann::json::parse(message);

    // Check if for this participant
    string current_participant_id = m["data"]["participant_id"];
    if (this->participant_id.compare(current_participant_id) == 0) {
        // Decode base64 chunk
        string coded_src = m["data"]["encoded"];
        int encoded_data_length = Base64decode_len(coded_src.c_str());
        vector<char> decoded(encoded_data_length);
        Base64decode(&decoded[0], coded_src.c_str());

        // Push chunk to spsc queue
        while (this->queue->push(decoded)) {
        }
    }
}
