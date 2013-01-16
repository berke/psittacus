struct options {
	bool debug;
	string path;
	string jid;
	string password;
	string talk_server;
	string sockpath;
	int port;
	vector<string> recipients;

	options() :
		debug(false),
		port(0)
	{ }
};

