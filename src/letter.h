struct letter {
	string to;
	string body;
	letter() { }
	letter(const string &Body) : body(Body) { }
	letter(const string &To, const string &Body) : to(To), body(Body) { }
	~letter() {}
	bool is_broadcast() const { return to.size() != 0; }
};
