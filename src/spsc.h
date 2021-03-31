#include <boost/lockfree/queue.hpp>
#include <vector>

extern boost::lockfree::spsc_queue<std::vector<float>,
                                   boost::lockfree::capacity<1024>>
    shared;
extern bool read_done;
extern bool write_start;
