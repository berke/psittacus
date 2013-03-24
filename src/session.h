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
		const vector<string> a =
			split_string(u.substr(1, u.size() - 1));
		if (a.size() == 0) {
			return fmt::spf("Empty command, try help.",
					u.c_str());
		}

		const string &w = a[0];
		if (!w.compare("help")) return cmd_help();
		if (!w.compare("history")) return cmd_history();
		//if (!w.compare("who")) return cmd_who();
		if (!w.compare("say")) return cmd_say(a);
		return fmt::spf("Unknown command '%s', try help.",
				w.c_str());
	}

	string cmd_say(const vector<string> &args) {
		if (args.size() != 2) return "Wrong number of args";

		stringstream u;
		if (false)
			u << "Message from " << jid.bare() << ":\n'"
				<< args[1] << "'";
		else
			u << "Message:\n'" << args[1] << "'";
		p->broadcast(letter("", u.str()));

		return "Message sent.";
	}

	string cmd_help() {
		return	"Dear friend, the following commands are "
			"available:\n"
			"  help - Display this\n"
			"  history - Display last messages\n"
			"  who - Show who is online\n"
			"  say [msg] - Broadcast a message";
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
