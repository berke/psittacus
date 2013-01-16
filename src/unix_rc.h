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

