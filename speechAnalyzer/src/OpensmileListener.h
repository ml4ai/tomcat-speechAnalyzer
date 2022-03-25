#pragma once

#include <string>

#include "OpensmileSession.h"
#include "JsonBuilder.h"
#include "arguments.h"
#include "Mosquitto.h"

class OpensmileListener : public Mosquitto {

  public:

    OpensmileListener(std::string mqtt_host_internal, int mqtt_port_internal, std::string participant_id, int socket_port);
    ~OpensmileListener();



  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    void Initialize();
    void Shutdown();
   
    std::string mqtt_host_internal;
    int mqtt_port_internal;
    
    std::string participant_id;
    std::thread listener_thread;

    int socket_port;
    OpensmileSession *session;
    JsonBuilder *builder;
};
