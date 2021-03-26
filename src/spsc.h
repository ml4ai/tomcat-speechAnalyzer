#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

extern boost::lockfree::spsc_queue<std::vector<float>, boost::lockfree::capacity<1024>> shared;
extern bool shared_done;
