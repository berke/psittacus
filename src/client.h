class client {
	pthread_t thread1, thread2;
	unix_fd fd;
	string message;
	vector<char> rx_buf, tx_buf;
	notif &notify;
	notification<letter> &to_broadcast;
	notification<letter> received;
	clit notif_clit;
	bool running;
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
		rx_buf(512),
		tx_buf(512),
		notify(Notify),
		to_broadcast(To_broadcast),
		running(true) {

		unix_rc rc;

		rc = pthread_create(&thread1, NULL, start1, this);
		try {
			rc = pthread_create(&thread2, NULL, start2, this);
		} catch(...) {
			void *retval;
			pthread_join(thread1, &retval);
			throw;
		}
	}

	void set_clit(clit it)
	{
		notif_clit = it;
	}

	virtual ~client() {
		void *retval;
		unix_rc rc;

		// Horrible business...
		
		pthread_kill(thread1, SIGUSR1);
		rc = pthread_join(thread1, &retval);

		received.cancel();
		pthread_kill(thread2, SIGUSR1);
		rc = pthread_join(thread2, &retval);
	}

	void receive(const letter &let) { received.put(let); }

private:
	void push_uint32_t(uint32_t data)
	{
		tx_buf.push_back((data >> 24) & 255);
		tx_buf.push_back((data >> 16) & 255);
		tx_buf.push_back((data >> 8) & 255);
		tx_buf.push_back(data & 255);
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
		nat i = tx_buf.size();
		tx_buf.resize(i + m);
		memcpy(&tx_buf[i], u.c_str(), m);
	}

	void transmit(void)
	{
		nat i = 0;

		nat count = tx_buf.size();

		while (i < count) {
			ssize_t n = write(fd, &tx_buf[i], count - i);
			if (n == 0) throw eof();
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK
						/*|| errno == EINTR*/)
					continue;
				throw runtime_error(fmt::spf("Transmit: %s",
						strerror(errno)));
			}
			i += n;
		}
	}

	/* This thread blockingly reads from the FD */
	void receive(int count)
	{
		int i = 0;

		rx_buf.resize(count);

		while (i < count) {
			ssize_t n = read(fd, &rx_buf[i], count - i);
			if (n == 0) throw eof();
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK
						/*|| errno == EINTR*/)
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
		return decode_uint32_t(&rx_buf[0]);
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
		string u(&rx_buf[0], m);
		fmt::spf("Str '%s'\n", u.c_str());
		return u;
	}

	void command(vector<string> &args)
	{
		nat n = args.size();

		if (args[0] == "say" && n == 3) {
			to_broadcast.put(letter(args[1], args[2]));
		} else {
			fmt::pf("Unknown command '%s' %d\n",
					args[0].c_str(), n);
		}
	}

	void loop() {
		vector<string> args;

		uint32_t m = receive_uint32_t();
		if (!m || m > args_max)
			throw runtime_error("Argument count invalid");
		args.resize(m);

		for (nat i = 0; i < m; i ++) args[i] = receive_string();

		command(args);
	}

	void transmit_response(vector<string> &response)
	{
		tx_buf.clear();
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

	void run1() {
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

	void run2() {
		while (true) {
			letter let;
			if (!received.wait(let)) break;
			fmt::pf("Received [%s] [%s]\n",
					let.to.c_str(),
					let.body.c_str());
			vector<string> response;
			response.resize(3);
			response[0] = "msg";
			response[1] = let.to.c_str();
			response[2] = let.body.c_str();
			transmit_response(response);
		}
	}

	static void *start1(void *arg) {
		reinterpret_cast<client*>(arg)->run1();
		return NULL;
	}

	static void *start2(void *arg) {
		reinterpret_cast<client*>(arg)->run2();
		return NULL;
	}
};

