// Remote control

#include "notification.h"

class client;

typedef list<shared_ptr<client>>::iterator clit;
typedef notification<clit> notif;

class client {
	pthread_t thread;
	unix_fd fd;
	string message;
	vector<char> buf;
	notif &notify;
	clit notif_clit;

public:
	client(int Fd, notif &Notify) :
		fd(Fd),
		buf(512),
		notify(Notify) {
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
	bool receive(int count)
	{
		int i = 0;

		buf.resize(count);

		while (i < count) {
			ssize_t n = read(fd, &buf[i], count - i);
			if (n == 0) {
				return false;
			}
			if (n < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK ||
						errno == EINTR)
					continue;
				throw runtime_error(fmt::spf("Recv: %s",
						strerror(errno)));
			}
			i += n;
		}
		return true;
	}

	void run() {
		while (true) {
			fmt::pf("Lay lay lom\n");
			if (!receive(8)) {
				fmt::pf("EOF!\n");
				notify.put(notif_clit);
				return;
			}
			fmt::pf("Got %02x %02x %02x %02x\n",
					buf[0], buf[1], buf[2], buf[3]);
		}
	}

	static void *start(void *arg) {
		reinterpret_cast<client*>(arg)->run();
		return NULL;
	}
};

class remote {
	unix_fd sk;
	list<shared_ptr<client>> clients;
	notif finished_clients;

public:
	remote(const char *path) {
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

	remote(int port) {
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

	virtual ~remote() {
	}

	void process() {
		process_accept();
		process_finish();
	}

	void process_finish() {
		clit it;
		if (finished_clients.get(it)) {
			fmt::pf("Deleting client %p\n", it);
			clients.erase(it);
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

		shared_ptr<client> cl(new client(c, finished_clients));

		clients.push_front(cl);
		cl->set_clit(clients.begin());
	}
};
