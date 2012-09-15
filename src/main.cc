// psittacus 1.0
//
// Copyright (C)2012 Berke DURAK

#include <vector>
#include <string>
#include <map>
#include <set>
#include <forward_list>
#include <vector>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <utility>

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cinttypes>
#include <cstdarg>
#include <cassert>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>

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

	options() :
		debug(false)
	{ }
};

void do_collect(const options &o, const char *progname)
{
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
		args.error();
	} while (!args.is_empty() || args.processing());

	return rc;
}

// vim:set sw=8 ts=8 noexpandtab:
