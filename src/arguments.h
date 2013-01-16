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

