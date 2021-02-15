#include <cerrno>  //errno
#include <cstring> //strerror
#include <iostream>
#include <vector>

const size_t MAX_CHUNK_SIZE = 2048;

void process_chunk(const std::vector<unsigned char>&);

int main() {
    try {
        std::freopen(nullptr, "rb", stdin); // reopen stdin in binary mode

        if (std::ferror(stdin)) {
            throw(std::runtime_error(std::strerror(errno)));
        }

        std::vector<unsigned char> buffer(MAX_CHUNK_SIZE);
        std::size_t length;
        while ((length = std::fread(&buffer[0], sizeof buffer[0], buffer.size(), stdin)) > 0) {
            std::vector<unsigned char> chunk(buffer.begin(),
                                             buffer.begin() + length);
            process_chunk(chunk);
        }
    }
    catch (std::exception const& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void process_chunk(const std::vector<unsigned char>& chunk) {
    std::cout << chunk.size() << std::endl;
}
