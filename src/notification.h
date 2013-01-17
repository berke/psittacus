template<typename t>
class notification {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	list<t> queue;
	bool cancelled;

public:
	notification() : cancelled(false) {
		pthread_mutex_init(&mutex, NULL);
		pthread_cond_init(&cond, NULL);
	}

	virtual ~notification() {
		pthread_cond_destroy(&cond);
		pthread_mutex_destroy(&mutex);
	}

	void cancel() {
		cancelled = true; // Not atomic...
		pthread_cond_broadcast(&cond);
	}

	void put(t elem) {
		unix_rc rc = pthread_mutex_lock(&mutex);
		try {
			fmt::pf("Notify push something\n");
			queue.push_back(elem);
			pthread_cond_broadcast(&cond);
			pthread_mutex_unlock(&mutex);
			fmt::pf("Unlocking...\n");
		} catch(...) {
			fmt::pf("Caught exception in put\n");
			pthread_mutex_unlock(&mutex);
			throw;
		}
	}

	bool wait(t &elem) {
		unix_rc rc = pthread_mutex_lock(&mutex);
		while (true) {
			fmt::pf("Locking...\n");
			try {
				if (cancelled) {
					pthread_mutex_unlock(&mutex);
					return false;
				}
				if (!queue.empty()) {
					fmt::pf("Queue not empty\n");
					elem = queue.front();
					queue.pop_front();
					pthread_mutex_unlock(&mutex);
					fmt::pf("Returning.....\n");
					return true;
				} else fmt::pf("Queue empty\n");
				fmt::pf("Notify wait...\n");
				rc = pthread_cond_wait(&cond, &mutex);
				fmt::pf("Notify woke up...\n");
			} catch(...) {
				fmt::pf("Caught something...\n");
				pthread_mutex_unlock(&mutex);
				throw;
			}
		}
	}

	bool get(t &elem) {
		bool ret = false;
		unix_rc rc = pthread_mutex_lock(&mutex);
		try {
			if (!queue.empty()) {
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
