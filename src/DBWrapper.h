#include <string>
#include <vector>

#include <libpq-fe.h>
#include <nlohmann/json.hpp>

class DBWrapper {
	public:
		DBWrapper();
		~DBWrapper();

		void initialize();
		void shutdown();		
		void publish_chunk(nlohmann::json message);

		std::string participant_id;
		double timestamp;

	private:
		static const std::vector<char> INVALID_COLUMN_CHARACTERS;
		std::string format_to_db_string(std::string in);

		std::string user = "postgres";
		std::string pass = "docker";
		std::string host = "features-db";
		std::string port = "63332";
		PGconn *conn;
};
