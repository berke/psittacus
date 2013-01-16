static vector<string> split_string(const string &u)
{
	string v;
	vector<string> a;
	unsigned i;
	char c;
	enum { white, word, quote, rest } q = white;

	for (i = 0; i <= u.size(); i ++) {
		c = i < u.size() ? u[i] : 0;
		switch (q) {
			case white:
				switch (c) {
					case 0:
					case ' ':
					case '\n':
					case '\t':
					case '\r':
						break;
					case '"':
						q = quote;
						break;
					case ':':
						q = rest;
						break;
					default:
						q = word;
						i --;
						continue;
				}
				break;
			case rest:
				switch (c) {
					case 0:
						a.push_back(v);
						v.clear();
						q = white;
						break;
					default:
						v += c;
						break;
				}
				break;
			case word:
				switch (c) {
					case 0:
					case ' ':
					case '\n':
					case '\t':
					case '\r':
						a.push_back(v);
						v.clear();
						q = white;
						break;
					case ':':
						a.push_back(v);
						v.clear();
						q = rest;
						break;
					case '"':
						a.push_back(v);
						v.clear();
						q = quote;
						break;
					default:
						v += c;
						break;
				}
				break;
			case quote:
				switch (c) {
					case 0:
						throw runtime_error(
							"Unterminated quote");
					case '"':
						a.push_back(v);
						v.clear();
						q = white;
						break;
					default:
						v += c;
						break;
				}
				break;
		}
	}

	return a;
}

