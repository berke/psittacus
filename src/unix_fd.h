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

