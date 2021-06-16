#pragma once

#include <string>

#include <boost/lockfree/spsc_queue.hpp>

#include "Mosquitto.h"

class ChunkListener : public Mosquitto {
  public:
    ChunkListener(
        std::string participant_id,
        boost::lockfree::spsc_queue<std::vector<char>,
                                    boost::lockfree::capacity<1024>>* queue);

  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    boost::lockfree::spsc_queue<std::vector<char>,
                                boost::lockfree::capacity<1024>>* queue;
    std::string participant_id;
};
