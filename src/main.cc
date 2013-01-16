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
#include <sys/time.h>
#include <netinet/in.h>

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

#include "unix_rc.h"
#include "non_copyable.h"
#include "unix_fd.h"
#include "fmt.h"
#include "arguments.h"
#include "options.h"

using namespace gloox;

namespace time_utils {
	double now(void) {
		struct timeval tv;

		unix_rc rc = gettimeofday(&tv, NULL);
		return tv.tv_sec + 1e-6 * tv.tv_usec;
	}
};

#include "split_string.h"
#include "parrot.h"
#include "seqpacket.h"

void do_parrot(const options &o, const char *progname)
{
	parrot p(o.jid, o.password, o.talk_server);
	seqpacket *sp;
	
	if (o.sockpath.size()) {
		sp = new seqpacket(o.sockpath.c_str());
	} else if (o.port > 0) {
		sp = new seqpacket(o.port);
	} else throw runtime_error("Socket path or TCP port must be specified");

	string message;

	for (auto &it: o.recipients) p.add_target(it);

	unix_rc rc = fcntl(0, F_SETFL, O_NONBLOCK);
	while (true) {
		p.run(10000);
		sp->process();
		if (sp->pending(message))
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
			args.run("Use a UNIX socket and set the path")
		) ||
		(
		 	args.pop_keyword("--port") &&
			args.pop_int(o.port) &&
			args.run("Use a TCP port and set the port")
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
