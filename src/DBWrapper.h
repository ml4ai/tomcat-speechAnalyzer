#include <string>
#include <vector>
#include <map>
#include <libpq-fe.h>
#include <nlohmann/json.hpp>

class DBWrapper {
	public:
		DBWrapper();
		~DBWrapper();

		void initialize();
		void shutdown();		
		void publish_chunk(nlohmann::json message);
		std::vector<nlohmann::json> features_between(double start_time, double end_time);

		std::string participant_id;
		double timestamp;

	private:
		static const std::vector<char> INVALID_COLUMN_CHARACTERS;
		std::map<std::string, std::string> column_map;
		std::string format_to_db_string(std::string in);

		std::string user = "postgres";
		std::string pass = "docker";
		std::string db = "features";
		std::string host = "features_db";
		std::string port = "5432";
		PGconn *conn;
};
