class parrot : public MessageHandler, public RosterListener {
	Client *j;
	double ping_interval;
	double t_start, t_last_ping;

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
			const vector<string> a = split_string(u);
			if (a.size() == 0) {
				return fmt::spf("Empty command, try help.",
						u.c_str());
			}

			const string &w = a[0];
			if (!w.compare("help")) return cmd_help();
			if (!w.compare("history")) return cmd_history();
			if (!w.compare("who")) return cmd_who();
			if (!w.compare("say")) return cmd_say(a);
			return fmt::spf("Unknown command '%s', try help.",
					w.c_str());
		}

		string cmd_say(const vector<string> &args) {
			if (args.size() != 2) return "Wrong number of args";

			stringstream u;
			u << "Message from " << jid.bare() << ":\n'"
				<< args[1] << "'";
			p->broadcast(u.str());

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
	map<string, session *> recipients;
	deque<string> history;
	const unsigned history_max_size;

public:
	parrot(const string &Jid, const string &Pwd, const string &Server) :
		ping_interval(20),
		t_start(time_utils::now()),
		t_last_ping(-1),
		history_max_size(25)
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
		fmt::pf("Broadcasting: %s\n", message.c_str());
		add_to_history(message);
		for (auto &it: recipients)
			it.second->send(message);
	}

	void ping() {
		double t = time_utils::now();
		if (t_last_ping >= 0 && t_last_ping + ping_interval > t)
			return;
		t_last_ping = t;
		fmt::pf("Pinging...\n");
		j->setPresence(Presence::Available, 0, status_string());
	}

	string status_string()
	{
		int dt = time_utils::now() - t_start;

		return fmt::spf("Up %d:%02d:%02d",
				dt / 3600,
				(dt % 3600) / 60,
				dt % 60);
	}

	void run(int timeout) {
		ping();
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