// psittacus 1.0
//
// Copyright (C)2012 Berke DURAK

#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include <queue>
#include <set>
#include <sstream>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cinttypes>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static inline int unix_bind(int sockfd, const struct sockaddr *addr,
		socklen_t addrlen)
{
	return bind(sockfd, addr, addrlen);
}

#include <gloox/client.h>
#include <gloox/message.h>
#include <gloox/messagehandler.h>
#include <gloox/messagesession.h>
#include <gloox/rosterlistener.h>
#include <gloox/rostermanager.h>

using namespace std;

class unix_rc {
public:
	unix_rc(int Rc) {
		check(Rc);
	}

	unix_rc() { }

	virtual ~unix_rc() { }

	int operator=(int Rc) {
		check(Rc);
		return Rc;
	}

	static void check(int Rc) {
		if (Rc < 0)
			error(NULL);
	}

	static void error(const char *detail) {
		throw runtime_error(
				string("Unix error: ")
				+ strerror(errno)
				+ (detail ?
					(string(" (") + detail + string(")")) :
					""));
	}
};

class non_copyable
{
protected:
	non_copyable() { }
	~non_copyable() { }
private:
	non_copyable(const non_copyable&);
	non_copyable &operator=(const non_copyable&);
};

class unix_fd : non_copyable {
	int fd;

public:
	unix_fd() : fd(-1) { }

	explicit unix_fd(int Fd) : fd(Fd) {
		unix_rc rc = fd;
	}

	virtual ~unix_fd() {
		close_this();
	}

	void close_this() {
		if (fd >= 0) {
			unix_rc rc = close(fd);
			fd = -1;
		}
	}

	void operator=(int Fd) {
		close_this();
		fd = Fd;
		unix_rc rc = Fd;
	}

	operator int() const {
		return fd;
	}

	int get() const { return fd; }
};

namespace fmt {
	static void vfpf(FILE *out, const char *fmt, va_list ap) {
		unix_rc rc = ::vfprintf(out, fmt, ap);
	}

	static void pf(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		vfpf(stdout, fmt, ap);
		va_end(ap);
	}

	static void fpf(FILE *out, const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		vfpf(out, fmt, ap);
		va_end(ap);
	}

	static string spf(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		char *u;
		unix_rc rc = vasprintf(&u, fmt, ap);
		va_end(ap);
		string v(u);
		free(u);
		return v;
	}

	static void fpc(char c, FILE *out) {
		fputc(c, out);
	}

	void print_quoted(FILE *out, const char *u)
	{
		fputc('\'', out);
		while (*u) {
			char c = *(u ++);
			if (32 <= c && c < 127) {
				if (c == '\'')
					fpf(out, "'\\''");
				else fpc(c, out);
			} else switch (c) {
				case '\n': fpf(out, "\\n"); break;
				case '\r': fpf(out, "\\r"); break;
				case '\b': fpf(out, "\\b"); break;
				default:
					   fpf(out, "\\%03o",
							   (unsigned int) c);
					   break;
			}
		}
		fpc('\'', out);
	}
};

class arguments {
	size_t i;
	vector<string> args;
	bool dry_run;
	const char *description;
	const char *progname;

public:
	class out_of_arguments : runtime_error {
		out_of_arguments() : runtime_error("Out of arguments") { }
	};

	arguments(int argc, const char * const * argv, const char *Description)
		:
		i(0),
		dry_run(false),
		description(Description) {
		progname = argv[0];
		args.resize(argc - 1);
		for (int j = 0; j < argc - 1; j ++)
			args[j] = argv[j + 1];
	}

	virtual ~arguments() { }

	bool processing() {
		return dry_run;
	}

	bool run(const char *msg) {
		if (dry_run) {
			fmt::fpf(stderr, " - %s\n", msg);
			return false;
		} else return true;
	}
	void dry() { i = 0; dry_run = true; }
	bool is_empty() { return i == args.size(); }
	string &front() { return args[i]; }

	bool pop_all(void) {
		i = args.size();
		return true;
	}

private:
	bool pop_int_fmt(int &o, const char *fmt) {
		if (dry_run) {
			fmt::fpf(stderr, " <integer>");
			return true;
		}
		if (is_empty()) return false;
		string u = front();
		int n;

		if (1 == sscanf(u.c_str(), fmt, &o, &n) &&
			u.c_str()[n] == 0) {
			pop();
			return true;
		} else return false;
	}

public:
	bool pop_int(int &o) {
		return pop_int_fmt(o, "%d%n");
	}

	bool pop_int_octal(int &o) {
		return pop_int_fmt(o, "%o%n");
	}

	bool pop_string_vector(const char *dsc, vector<string> &v) {
		if (dry_run) {
			fmt::fpf(stderr,
				" (<%s:string>|'' <%s:string> ... '')",
				dsc, dsc);
			return true;
		}

		string u = front(); pop();

		if (u.size() != 0) {
			v.push_back(u);
			return true;
		}

		while (!is_empty()) {
			string u = front(); pop();
			if (u.size() == 0)
				return true;
			v.push_back(u);
		}
		
		return true;
	}

	bool pop_empty_string() {
		if (dry_run) {
			fmt::fpf(stderr, " ''");
			return true;
		}

		if (!is_empty() && front().size() == 0) {
			pop();
			return true;
		} else return false;
	}

	bool pop_string(const char *dsc, string &u) {
		if (dry_run) {
			fmt::fpf(stderr, " <%s:string>", dsc);
			return true;
		}
		if (is_empty()) return false;
		u = front(); pop();
		return true;
	}

	bool pop_keyword(const char *u) {
		if (dry_run) {
			fmt::fpf(stderr, " %s", u);
			return true;
		}
		if (is_empty()) return false;
		return front().compare(u) == 0 && pop();
	}

	bool pop_keyword(const char *u1, const char *u2) {
		if (dry_run) {
			fmt::fpf(stderr, " (%s|%s)", u1, u2);
			return true;
		}
		if (is_empty()) return false;
		return (front().compare(u1) == 0 || front().compare(u2) == 0)
			&& pop();
	}

	bool pop() {
		if (i < args.size()) {
			i ++;
			return true;
		} else return false;
	}

	bool usage() {
		if (dry_run) {
			dry_run = false;
			return true;
		}

		fmt::fpf(stderr, "Usage: %s %s\nArguments:\n", progname,
				description);
		dry();
		pop_all();

		return true;
	}

	bool error()
	{
		if (dry_run) {
			dry_run = false;
			return true;
		}

		if (is_empty()) {
			fmt::fpf(stderr, "Error: missing argument\n");
		} else {
			fmt::fpf(stderr, "Error: Unknown argument "
					"#%zu/%zu '%s'\n",
					i,
					args.size(),
					args[i].c_str()
			);
		}
		return usage();
	}
};

struct options {
	bool debug;
	string path;
	string jid;
	string password;
	string talk_server;
	string sockpath;
	vector<string> recipients;

	options() :
		debug(false),
		sockpath("parrot")
	{ }
};

using namespace gloox;

class parrot : public MessageHandler, public RosterListener {
	Client *j;
	class session {
		parrot *p;
		Client *j;
		JID jid;
		MessageSession *ms;

	public:
		session(parrot *P, Client *J, const string &Target) :
			p(P), j(J), jid(Target) {
			fmt::pf("Registering new target %s\n", Target.c_str());

			ms = new MessageSession(&*j, jid);
			ms->registerMessageHandler(P);
		}

		virtual ~session() {
			ms->removeMessageHandler();
		}

		void send(const string &msg) {
			ms->send(msg);
		}

		string answer(const string &u) {
			if (!u.compare("help")) return cmd_help();
			if (!u.compare("history")) return cmd_history();
			if (!u.compare("who")) return cmd_who();
			return fmt::spf("Unknown command %s, try help.",
					u.c_str());
		}

		string cmd_help() {
			return	"Dear friend, the following commands are "
				"available:\n"
				"  help - Display this\n"
				"  history - Display last messages\n"
				"  who - Show who is online";
		}

		string cmd_history() {
			stringstream u;
			if (p->history.size() == 0) return "No messages yet.";
			u << "Last messages:";
			for (auto &it: p->history) u << "\n" << it;
			return u.str();
		}

		string cmd_who() {
			stringstream u;
			u << "Users:";
			for (auto &it: p->recipients) u << "\n  " << it.first;
			return u.str();
		}
	};
	map<string, session *> recipients;
	deque<string> history;
	const unsigned history_max_size;

public:
	parrot(const string &Jid, const string &Pwd, const string &Server) :
		history_max_size(5)
	{
		JID jid(Jid);
		j = new Client(jid, Pwd);
		j->registerMessageHandler(this);
		j->rosterManager()->registerRosterListener(this);
		if (Server.size() > 0) j->setServer(Server);
		fmt::pf("Connecting as %s %s\n", Jid.c_str(), Pwd.c_str());
		if (!j->connect(false))
			throw runtime_error("Connection failed");
		fmt::pf("Connected\n");
	}

	virtual ~parrot() {
	}

	void handleMessage(const Message &stanza, MessageSession *sess)
	{
		const string &what = stanza.body();
		const string &from = stanza.from().bare();
		fmt::pf("From %s: %s\n", from.c_str(),
				what.c_str());

		auto t = stanza.subtype();
		if (t & Message::MessageType::Error) {
			fmt::pf("  Error\n");
		} else if (t & Message::MessageType::Chat) {
			string response;
			auto r = recipients.find(from);

			if (r == recipients.end()) {
				fmt::pf("  Unregistered\n");
				response = "Please friend me before talking.";
			} else {
				response = r->second->answer(what);
			}
			Message answer(
					Message::MessageType::Chat,
					stanza.from(),
					response);
			j->send(answer);
		} else {
			fmt::pf("  Unhandled message type %d\n", t);
		}
	}

	void add_target(const string &target) {
		if (recipients.find(target) != recipients.end()) return;
		recipients[target] = new session(this, j, target);
	}

	void remove_target(const string &target) {
		recipients.erase(target);
	}

	void add_to_history(const string &message) {
		history.push_back(message);
		if (history.size() == history_max_size) history.pop_front();
	}

	void broadcast(const string &message) {
		add_to_history(message);
		for (auto &it: recipients)
			it.second->send(message);
	}

	void run(int timeout) {
		j->recv(timeout);
	}

	bool handleSubscriptionRequest(const JID &jid, const string &msg)
	{
		fmt::pf("Got autho req %s\n", msg.c_str());
		add_target(jid.bare());
		return true;
	}

	bool handleUnsubscriptionRequest(const JID &jid, const string &msg)
	{
		fmt::pf("Got unautho req %s\n", msg.c_str());
		remove_target(jid.bare());
		return true;
	}

	void handleItemAdded(const JID &jid)
	{
		fmt::pf("Item added\n");
	}

	void handleItemSubscribed(const JID &jid)
	{
		fmt::pf("Item subscribed\n");
	}

	void handleItemRemoved(const JID &jid)
	{
		fmt::pf("Item removed\n");
	}

	void handleItemUpdated(const JID &jid)
	{
		fmt::pf("Item updated\n");
	}

	void handleItemUnsubscribed(const JID &jid)
	{
		fmt::pf("Item unsubscribed\n");
	}

	void handleRoster(const Roster &roster)
	{
		fmt::pf("Roster\n");
		for (auto &it: roster) {
			fmt::pf("  %s\n", it.first.c_str());
			add_target(it.first);
		}
	}

	void handleRosterPresence(const RosterItem &item,
			const string &resource,
			Presence::PresenceType presence,
			const string &msg)
	{
		fmt::pf("Roster presence\n");
	}

	void handleSelfPresence(const RosterItem &item, const string
			&resource, Presence::PresenceType presence,
			const string &msg)
	{
		fmt::pf("Roster self presence: msg=%s\n", msg.c_str());
	}

	void handleNonrosterPresence(const Presence &presence)
	{
		fmt::pf("Nonroster presence\n");
	}

	void handleRosterError(const IQ &iq)
	{
		fmt::pf("Roster error\n");
	}
};

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

void do_parrot(const options &o, const char *progname)
{
	parrot p(o.jid, o.password, o.talk_server);
	seqpacket sp(o.sockpath.c_str());
	string message;

	for (auto &it: o.recipients) p.add_target(it);

	unix_rc rc = fcntl(0, F_SETFL, O_NONBLOCK);
	while (true) {
		p.run(10000);
		sp.process();
		if (sp.pending(message))
			p.broadcast(message);
	}
}

static const char *description =
	"[options]\n"
	"\n"
	"Blah blah blah."
	"\n"
	;

int main(int argc, const char * const * argv)
{
	int rc = 0;
	options o;
	string u;

	arguments args(argc, argv, description);

	do {
		(
		 	args.pop_keyword("-h", "--help") &&
			args.run("Print help") &&
			args.usage()
		) ||
		(
			args.pop_keyword("-v", "--version") &&
			args.run("Show version") &&
			(fmt::pf("psittacus compiled on " 
				__DATE__ " at " __TIME__ "\n"
				"Copyright (C)2012 Berke DURAK "
				"<berke.durak@gmail.com>\n"
				"Released under the GNU GPL v3.\n"),
			 true)
		) ||
		(
		 	args.pop_keyword("--debug") &&
			args.run("Enable debuging") &&
			(o.debug = true, true)
		) ||
		(
		 	args.pop_keyword("-j", "--jid") &&
			args.pop_string("jid", o.jid) &&
			args.run("Set JID for logging in")
		) ||
		(
		 	args.pop_keyword("-t", "--talk-server") &&
			args.pop_string("server", o.talk_server) &&
			args.run("Set XMPP talk server")
		) ||
		(
		 	args.pop_keyword("-p", "--password") &&
			args.pop_string("pwd", o.password) &&
			args.run("Set password for logging in")
		) ||
		(
		 	args.pop_keyword("-r", "--recipients") &&
			args.pop_string_vector("recipients", o.recipients) &&
			args.run("Give list of recipients")
		) ||
		(
		 	args.pop_keyword("-s", "--sockpath") &&
			args.pop_string("path", o.sockpath) &&
			args.run("Set UNIX socket path")
		) ||
		(
			args.pop_keyword("--run") &&
			args.run("Run parrot") &&
			args.is_empty() &&
			(do_parrot(o, argv[0]), true)
		) ||
		args.error();
	} while (!args.is_empty() || args.processing());

	return rc;
}

// vim:set sw=8 ts=8 noexpandtab:
