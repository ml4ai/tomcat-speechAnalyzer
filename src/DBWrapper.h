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

	private:
		static const std::vector<char> INVALID_COLUMN_CHARACTERS;
		std::string format_to_db_string(std::string in);

		std::string db = "features";
		std::string user = "postgres";
		std::string pass = "docker";
		PGconn *conn;
};
