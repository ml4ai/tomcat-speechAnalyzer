#include <string>

#include "OpensmileSession.h"
#include "JsonBuilder.h"

#include "Mosquitto.h"

class OpensmileListener : public Mosquitto {
  static int socket_port = 10000;
  int port = -1;

  public:

    OpensmileListener(Arguments args, std::string participant_id);
    ~OpensmileListener();



  protected:
    void on_message(const std::string& topic,
                    const std::string& message) override;

  private:
    void Initialize();
    void Shutdown();
   
    std::string participant_id;
    std::thread listener_thread;

    OpensmileSession *session;
    JsonBuilder *builder;
};
