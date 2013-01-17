class parrot : public MessageHandler, public RosterListener,
	public ConnectionListener {
	Client *j;
	double ping_interval;
	double t_start, t_last_ping;
	bool connected;
	notification<letter> received;

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
	map<string, session *> recipients;
	deque<string> history;
	const unsigned history_max_size;

public:
	struct disconnected { };

	parrot(const string &Jid, const string &Pwd, const string &Server) :
		ping_interval(20),
		t_start(time_utils::now()),
		t_last_ping(-1),
		history_max_size(25)
	{
		JID jid(Jid);
		j = new Client(jid, Pwd);
		j->registerMessageHandler(this);
		j->registerConnectionListener(this);
		j->rosterManager()->registerRosterListener(this);
		if (Server.size() > 0) j->setServer(Server);
		fmt::pf("Connecting as %s %s\n", Jid.c_str(), Pwd.c_str());
		if (!j->connect(false))
			throw disconnected();
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
				if (what.size() > 0 && what[0] == '\\')
					response = r->second->answer(what);
				else {
					// Pass it
					letter let(from, what);
					received.put(let);
				}
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

	bool pending(letter &let) {
		return received.get(let);
	}

	void broadcast(const letter &let) {
		if (let.is_broadcast()) {
			fmt::pf("Sending to [%s]: [%s]\n",
				let.to.c_str(), let.body.c_str());
			auto it = recipients.find(let.to);
			if (it == recipients.end()) {
				// That didn't work out
				fmt::pf("Error: can't find recipient %s\n",
						let.to.c_str());
			} else it->second->send(let.body);
		} else {
			fmt::pf("Broadcasting: [%s]\n", let.body.c_str());
			add_to_history(let.body);
			for (auto &it: recipients)
				it.second->send(let.body);
		}
	}

	void ping() {
		if (!connected) return;
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
		j->recv(timeout);
		ping();
	}

	void onConnect() {
		fmt::pf("Connected...\n");
	}

	string string_of_stream_error(StreamError error) {
		string msg;

		switch (error) {
			case StreamErrorBadFormat:
				msg = "StreamErrorBadFormat"; break;
			case StreamErrorBadNamespacePrefix:
				msg = "StreamErrorBadNamespacePrefix"; break;
			case StreamErrorConflict:
				msg = "StreamErrorConflict"; break;
			case StreamErrorConnectionTimeout:
				msg = "StreamErrorConnectionTimeout"; break;
			case StreamErrorHostGone:
				msg = "StreamErrorHostGone"; break;
			case StreamErrorHostUnknown:
				msg = "StreamErrorHostUnknown"; break;
			case StreamErrorImproperAddressing:
				msg = "StreamErrorImproperAddressing"; break;
			case StreamErrorInternalServerError:
				msg = "StreamErrorInternalServerError"; break;
			case StreamErrorInvalidFrom:
				msg = "StreamErrorInvalidFrom"; break;
			case StreamErrorInvalidId:
				msg = "StreamErrorInvalidId"; break;
			case StreamErrorInvalidNamespace:
				msg = "StreamErrorInvalidNamespace"; break;
			case StreamErrorInvalidXml:
				msg = "StreamErrorInvalidXml"; break;
			case StreamErrorNotAuthorized:
				msg = "StreamErrorNotAuthorized"; break;
			case StreamErrorPolicyViolation:
				msg = "StreamErrorPolicyViolation"; break;
			case StreamErrorRemoteConnectionFailed:
				msg = "StreamErrorRemoteConnectionFailed"; break;
			case StreamErrorResourceConstraint:
				msg = "StreamErrorResourceConstraint"; break;
			case StreamErrorRestrictedXml:
				msg = "StreamErrorRestrictedXml"; break;
			case StreamErrorSeeOtherHost:
				msg = "StreamErrorSeeOtherHost"; break;
			case StreamErrorSystemShutdown:
				msg = "StreamErrorSystemShutdown"; break;
			case StreamErrorUndefinedCondition:
				msg = "StreamErrorUndefinedCondition"; break;
			case StreamErrorUnsupportedEncoding:
				msg = "StreamErrorUnsupportedEncoding"; break;
			case StreamErrorUnsupportedStanzaType:
				msg = "StreamErrorUnsupportedStanzaType"; break;
			case StreamErrorUnsupportedVersion:
				msg = "StreamErrorUnsupportedVersion"; break;
			case StreamErrorXmlNotWellFormed:
				msg = "StreamErrorXmlNotWellFormed"; break;
			case StreamErrorUndefined:
				msg = "StreamErrorUndefined"; break;
		}
		return msg;
	}

	string string_of_connection_error(ConnectionError error)
	{
		string msg;
		switch (error) {
			case ConnNoError:
				msg = "ConnNoError"; break;
			case ConnNotConnected:
				msg = "ConnNotConnected"; break;
			case ConnStreamError:
				msg = "Connection stream error: " +
					string_of_stream_error(
						j->streamError());
				break;
			case ConnStreamVersionError:
				msg = "ConnStreamVersionError"; break;
			case ConnStreamClosed:
				msg = "ConnStreamClosed"; break;
			case ConnProxyAuthRequired:
				msg = "ConnProxyAuthRequired"; break;
			case ConnProxyAuthFailed:
				msg = "ConnProxyAuthFailed"; break;
			case ConnProxyNoSupportedAuth:
				msg = "ConnProxyNoSupportedAuth"; break;
			case ConnIoError:
				msg = "ConnIoError"; break;
			case ConnParseError:
				msg = "ConnParseError"; break;
			case ConnConnectionRefused:
				msg = "ConnConnectionRefused"; break;
			case ConnDnsError:
				msg = "ConnDnsError"; break;
			case ConnOutOfMemory:
				msg = "ConnOutOfMemory"; break;
			case ConnNoSupportedAuth:
				msg = "ConnNoSupportedAuth"; break;
			case ConnTlsFailed:
				msg = "ConnTlsFailed"; break;
			case ConnTlsNotAvailable:
				msg = "ConnTlsNotAvailable"; break;
			case ConnCompressionFailed:
				msg = "ConnCompressionFailed"; break;
			case ConnAuthenticationFailed:
				msg = "ConnAuthenticationFailed"; break;
			case ConnUserDisconnected:
				msg = "ConnUserDisconnected"; break;
			default:
				msg = "Unspecified error";
				break;
		}

		return msg;
	}


	void onDisconnect(ConnectionError error) {
		connected = false;
		fmt::pf("Disconnected : %s...\n",
			string_of_connection_error(error).c_str());
		throw disconnected();
	}

	void onStreamEvent(StreamEvent event) {
		fmt::pf("Stream event...\n");
	}

	bool onTLSConnect(const CertInfo &cert) {
		fmt::pf("TLS connect...\n");
		return true;
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
		connected = true;
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
