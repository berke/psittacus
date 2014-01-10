struct options {
	bool debug;
	string path;
	string jid;
	string password;
	string talk_server;
	string sockpath;
	int port;
	string remote_control_path;
	int remote_control_port;
	vector<string> recipients;

	options() :
		debug(false),
		port(0),
		remote_control_port(-1)
	{ }
};

