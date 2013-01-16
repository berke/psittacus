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

	static string spf(const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		char *u;
		unix_rc rc = vasprintf(&u, fmt, ap);
		va_end(ap);
		string v(u);
		free(u);
		return v;
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
