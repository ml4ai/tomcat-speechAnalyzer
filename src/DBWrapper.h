#include <libpq-fe.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>

class DBWrapper {
  public:
    DBWrapper();
    ~DBWrapper();

    void initialize();
    void shutdown();
    PGconn *get_connection();
    void publish_chunk(nlohmann::json message);
    std::vector<nlohmann::json> features_between(double start_time,
                                                 double end_time);

    std::string participant_id;
    std::string trial_id;
    double timestamp;

  private:
    static const std::vector<char> INVALID_COLUMN_CHARACTERS;
    std::map<std::string, std::string> column_map;
    std::string format_to_db_string(std::string in);

    std::string client_id;

    std::string connection_string;
    std::string user = "postgres";
    std::string pass = "docker";
    std::string db = "features";
    std::string host = "features_db";
    std::string port = "5432";

    int thread_pool_size=20;
    std::vector<PGconn*> connection_pool;

    bool running = false;
    std::thread publishing_thread;
    std::queue<nlohmann::json> publishing_queue;
    std::mutex publishing_mutex;
 
    void publish_chunk_private(nlohmann::json message);
    void loop();
};
