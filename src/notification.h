template<typename t>
class notification {
	pthread_mutex_t mutex;
	list<t> queue;
public:
	notification() {
		unix_rc rc = pthread_mutex_init(&mutex, NULL);
	}

	virtual ~notification() {
		unix_rc rc = pthread_mutex_destroy(&mutex);
	}

	void put(t elem) {
		unix_rc rc = pthread_mutex_lock(&mutex);
		try {
			fmt::pf("Notify push something\n");
			queue.push_back(elem);
		} catch(...) {
			pthread_mutex_unlock(&mutex);
			throw;
		}
		pthread_mutex_unlock(&mutex);
	}

	bool get(t &elem) {
		bool ret = false;
		unix_rc rc = pthread_mutex_lock(&mutex);
		try {
			if (!queue.empty()) {
				fmt::pf("Notify got something\n");
				elem = queue.front();
				queue.pop_front();
				ret = true;
			}
		} catch(...) {
			pthread_mutex_unlock(&mutex);
			throw;
		}
		pthread_mutex_unlock(&mutex);
		return ret;
	}
};
