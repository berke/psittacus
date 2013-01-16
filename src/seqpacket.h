class seqpacket {
	unix_fd sk;
	queue<string> messages;
	class client {
		unix_fd fd;
		string message;
		vector<char> buffer;
		seqpacket &parent;

	public:
		client(int Fd, seqpacket &Parent) : fd(Fd), buffer(512),
			parent(Parent) {
		}

		virtual ~client() {
		}

		bool process() {
			ssize_t n = read(fd, &buffer[0], buffer.size());
			if (n == 0) {
				parent.queue_message(message);
				return false;
			}
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK ||
						errno == EINTR)
					return true;
				throw runtime_error(fmt::spf("Recv: %s",
						strerror(errno)));
			}
			message.append(&buffer[0], n);
			return true;
		}
	};
	list<shared_ptr<client>> clients;

public:
	seqpacket(const char *path) {
		unix_rc rc;
		sk = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);

		struct sockaddr_un su;
		su.sun_family = AF_UNIX;
		strncpy(su.sun_path, path, sizeof(su.sun_path));
		su.sun_path[sizeof(su.sun_path) - 1] = 0;
		unlink(su.sun_path);
		rc = unix_bind(sk, reinterpret_cast<struct sockaddr *>(&su),
				sizeof(su));
		rc = listen(sk, 16);
	}

	seqpacket(int port) {
		unix_rc rc;
		sk = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

		struct sockaddr_in si;
		si.sin_family = AF_INET;
		si.sin_port = htons(port);
		si.sin_addr.s_addr = INADDR_ANY;
		rc = unix_bind(sk, reinterpret_cast<struct sockaddr *>(&si),
				sizeof(si));
		rc = listen(sk, 16);
	}

	virtual ~seqpacket() {
	}

	void process() {
		process_accept();
		auto it = clients.begin();
		while (it != clients.end()) {
			auto it_next = it;
			it_next ++;
			if (!(*it)->process()) clients.erase(it);
			it = it_next;
		}
	}

	void process_accept() {
		struct sockaddr_un su;
		socklen_t su_len;
		int c = accept4(sk, reinterpret_cast<struct sockaddr *>(&su),
				&su_len, SOCK_NONBLOCK);
		if (c < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK ||
					errno == EINTR)
				return;
			throw runtime_error(fmt::spf("Accept on %s: %s",
						su.sun_path, strerror(errno)));
		}
		clients.push_back(shared_ptr<client>(new client(c, *this)));
	}

	void queue_message(string &message) {
		messages.push(message);
	}

	bool pending(string &message) {
		if (messages.size() > 0) {
			message = messages.front();
			messages.pop();
			return true;
		} else return false;
	}
};

