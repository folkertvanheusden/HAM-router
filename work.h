#include <condition_variable>
#include <mutex>
#include <queue>


class tranceiver;

typedef struct {
	std::condition_variable  work_cv;
	std::mutex               work_lock;
	std::queue<tranceiver *> work_list;
} work_queue_t;
