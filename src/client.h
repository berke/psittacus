class client {
	pthread_t thread;
	unix_fd fd;
	string message;
	vector<char> buf;
	notif &notify;
	notification<letter> &to_broadcast;
	clit notif_clit;
	struct eof : public exception {
		eof() { }
		virtual ~eof() throw () { }
		const char *what() const throw () {
			return "EOF";
		}
	};
	enum { receive_max = 131072, args_max = 32 };

public:
	client(int Fd, notif &Notify, notification<letter> &To_broadcast) :
		fd(Fd),
		buf(512),
		notify(Notify),
		to_broadcast(To_broadcast) {
		unix_rc rc = pthread_create(&thread, NULL, start, this);
	}

	void set_clit(clit it)
	{
		notif_clit = it;
	}

	virtual ~client() {
		fmt::pf("Finalizing client %p\n", this);
		void *retval;
		unix_rc rc = pthread_join(thread, &retval);
	}

private:
	void push_uint32_t(uint32_t data)
	{
		buf.push_back((data >> 24) & 255);
		buf.push_back((data >> 16) & 255);
		buf.push_back((data >> 8) & 255);
		buf.push_back(data & 255);
	}

	uint32_t decode_uint32_t(const char *buf)
	{
		return
			(((uint32_t) buf[0] & 255) << 24) |
			(((uint32_t) buf[1] & 255) << 16) |
			(((uint32_t) buf[2] & 255) << 8) |
			((uint32_t) buf[3] & 255);
	}

	void push_string(const string &u)
	{
		nat m = u.length();
		push_uint32_t(m);
		nat i = buf.size();
		buf.resize(i + m);
		memcpy(&buf[i], u.c_str(), m);
	}

	void transmit(void)
	{
		nat i = 0;

		nat count = buf.size();
		fmt::pf("Transmitting %u\n", count);

		while (i < count) {
			fmt::pf("Transmit %u %u\n", i, count);
			ssize_t n = write(fd, &buf[i], count - i);
			if (n == 0) throw eof();
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK ||
						errno == EINTR)
					continue;
				throw runtime_error(fmt::spf("Transmit: %s",
						strerror(errno)));
			}
			i += n;
		}
		fmt::pf("Transmit done\n");
	}

	void receive(int count)
	{
		int i = 0;

		buf.resize(count);

		while (i < count) {
			ssize_t n = read(fd, &buf[i], count - i);
			if (n == 0) throw eof();
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK ||
						errno == EINTR)
					continue;
				throw runtime_error(fmt::spf("Recv: %s",
						strerror(errno)));
			}
			i += n;
		}
	}

	uint32_t receive_uint32_t()
	{
		receive(4);
		return decode_uint32_t(&buf[0]);
	}

	string receive_string()
	{
		nat m = receive_uint32_t();
		if (m > receive_max)
			throw runtime_error(
				fmt::spf("Packet size exceeded: %u > %u",
					m,
					receive_max));
		receive(m);
		string u(&buf[0], m);
		fmt::spf("Str '%s'\n", u.c_str());
		return u;
	}

	vector<string> command(vector<string> &args)
	{
		vector<string> result;
		nat n = args.size();

		if (args[0] == "hello") {
			result.resize(2);
			result[0] = "hi";
			result[1] = "markus";
		} else if (args[0] == "say" && n == 3) {
			to_broadcast.put(letter(args[1], args[2]));
			result.resize(1);
			result[0] = "ok";
		} else {
			fmt::pf("Unknown command '%s' %d\n",
					args[0].c_str(), n);
			result.resize(2);
			result[0] = "error";
			result[1] = "unknown-command";
		}

		return result;
	}

	void loop() {
		vector<string> args;

		uint32_t m = receive_uint32_t();
		if (!m || m > args_max)
			throw runtime_error("Argument count invalid");
		args.resize(m);

		for (nat i = 0; i < m; i ++) args[i] = receive_string();

		vector<string> response = command(args);

		buf.clear();
		nat n = response.size();
		push_uint32_t(n);
		fmt::pf("Response: %u {", n);

		for (nat j = 0; j < n; j ++) {
			fmt::pf(" '%s'", response[j].c_str());
			push_string(response[j]);
		}
		fmt::pf(" }\n");
		transmit();
	}

	void run() {
		try {
			int flag;

			unix_rc rc = flag = fcntl(fd, F_GETFL, 0);
			rc = fcntl(fd, F_SETFL, flag & ~O_NONBLOCK);

			while (true) {
				loop();
			}
		}
		catch(exception& e) {
			fmt::pf("Got exception: %s\n", e.what());
		}
		catch(...) {
		}
		notify.put(notif_clit);
	}

	static void *start(void *arg) {
		reinterpret_cast<client*>(arg)->run();
		return NULL;
	}
};

