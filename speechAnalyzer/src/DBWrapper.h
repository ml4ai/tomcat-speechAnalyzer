#include <libpq-fe.h>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <queue>
#include <string>
#include <thread>
#include <vector>

class DBWrapper {
  public:
    DBWrapper();
    ~DBWrapper();

    void initialize();
    void shutdown();
    PGconn* get_connection();
    void publish_chunk(nlohmann::json message);
    std::vector<nlohmann::json> features_between(double start_time,
                                                 double end_time, std::string participant_id, std::string trial_id);

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

    int thread_pool_size = 1;
    std::vector<std::thread> thread_pool;
    std::vector<PGconn*> connection_pool;
    std::vector<std::queue<nlohmann::json>> queue_pool;

    bool running = false;

    void publish_chunk_private(nlohmann::json message, int index);
    void loop(int index);
    void InitializeColumnMap();
};
