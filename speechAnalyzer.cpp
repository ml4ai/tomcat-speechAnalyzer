#include <iostream>
#include <vector>
const size_t MAX_CHUNK_SIZE = 128;

void process_chunk(std::vector<unsigned char>);

int main(){	
	
	std::freopen(nullptr, "rb", stdin); //reopen stdin in binary mode
	
	std::array<char, MAX_CHUNK_SIZE> buffer;
	std::vector<char> chunk;
	std::size_t length;
	while((length = std::fread(&chunk[0], sizeof chunk[0], chunk.size(), stdin)) > 0){
		if(chunk.size() < MAX_CHUNK_SIZE){
			chunk.push_back(input);
		}
		else{
			process_chunk(chunk);
		}
	}
	process_chunk(chunk);
}

void process_chunk(std::vector<unsigned char> chunk){
	std::cout << chunk.size() << std::endl;
}
