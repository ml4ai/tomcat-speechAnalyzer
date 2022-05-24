#pragma once 

// STDLIB
#include <condition_variable>
#include <map>
#include <mutex>
#include <queue>
#include <stack>
#include <string>
#include <thread>
#include <vector>

// Third party 
#include <nlohmann/json.hpp>
#include <libpq-fe.h>

class DBWrapper {
  public:
    DBWrapper(int connection_pool_size);

    void PublishChunk(nlohmann::json message);
    void ClearTrial(std::string trial_id);
    std::vector<nlohmann::json> FeaturesBetween(double start_time,
                                                 double end_time,
                                                 std::string participant_id,
                                                 std::string trial_id);

  private:

    std::map<std::string, std::string> column_map;
    
    void InitializeColumnMap();
    void InitializeConnections();
    std::string format_to_db_string(std::string in);

    // DB connection info
    std::string user = "postgres";
    std::string pass = "docker";
    std::string db = "features";
    std::string host = "features_db";
    std::string port = "5432";

    // Connection data
    int connection_pool_size;
    int active_connections = 0;
    std::stack<PGconn*> connection_pool;
    std::mutex connection_mutex;
    std::condition_variable connection_condition;
    
    PGconn* GetConnection();
    void FreeConnection(PGconn* conn);
    bool ConnectionAvaliable();

};
