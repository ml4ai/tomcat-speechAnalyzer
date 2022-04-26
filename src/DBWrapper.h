#include <libpq-fe.h>
#include <nlohmann/json.hpp>
#include <map>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <stack>

class DBWrapper {
  public:
    DBWrapper();
    ~DBWrapper();

    void Initialize();
    void Shutdown();
    
    void publish_chunk(nlohmann::json message);
    std::vector<nlohmann::json> features_between(double start_time,
                                                 double end_time, std::string participant_id, std::string trial_id);

  private:

    // Util code for handling postgres and opensmile formats 
    static const std::vector<char> INVALID_COLUMN_CHARACTERS;
    std::map<std::string, std::string> column_map;
    void InitializeColumnMap();
    std::string format_to_db_string(std::string in);


    // DB connection info
    std::string client_id;
    std::string connection_string;
    std::string user = "postgres";
    std::string pass = "docker";
    std::string db = "features";
    std::string host = "features_db";
    std::string port = "5432";

    // Connection data
    int connection_pool_size = 10;
    int active_connections = 0;
    std::stack<PGconn*> connection_pool;
    std::mutex connection_mutex;
    std::condition_variable connection_condition;
    PGconn* GetConnection();
    void FreeConnection(PGconn* conn);
    bool ConnectionAvaliable();

    bool running = false;

    void publish_chunk_private(nlohmann::json message);
};
