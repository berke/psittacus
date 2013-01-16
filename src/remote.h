// Remote control

#include "notification.h"

class client;

typedef list<shared_ptr<client>>::iterator clit;
typedef notification<clit> notif;

#include "client.h"

class remote {
	unix_fd sk;
	list<shared_ptr<client>> clients;
	notif finished_clients;
	notification<string> to_broadcast;

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
		int opt = 1;
		rc = setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

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

		shared_ptr<client>
			cl(new client(c, finished_clients, to_broadcast));

		clients.push_front(cl);
		cl->set_clit(clients.begin());
	}

	bool pending(string &message) {
		return to_broadcast.get(message);
	}
};
