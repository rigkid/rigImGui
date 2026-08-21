#include "ImGuiExpr.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace rigkit {
namespace {

void skipWs(const char*& s) {
	while (*s == ' ' || *s == '\t') {
		++s;
	}
}

bool parseExpr(const char*& s, double current, double& out);

bool parseNumber(const char*& s, double& out) {
	char* end = nullptr;
	out = std::strtod(s, &end);
	if (end == s) {
		return false;
	}
	s = end;
	return true;
}

bool parseCurrentX(const char*& s, double current, double& out) {
	if (*s != 'x' && *s != 'X') {
		return false;
	}
	const unsigned char next = static_cast<unsigned char>(s[1]);
	if (std::isalnum(next) || next == '_') {
		return false;
	}
	++s;
	out = current;
	return true;
}

bool parseFactor(const char*& s, double current, double& out) {
	skipWs(s);
	if (*s == '(') {
		++s;
		if (!parseExpr(s, current, out)) {
			return false;
		}
		skipWs(s);
		if (*s != ')') {
			return false;
		}
		++s;
		return true;
	}
	if (*s == '+') {
		++s;
		return parseFactor(s, current, out);
	}
	if (*s == '-') {
		++s;
		double v = 0.0;
		if (!parseFactor(s, current, v)) {
			return false;
		}
		out = -v;
		return true;
	}
	if (parseCurrentX(s, current, out)) {
		return true;
	}
	return parseNumber(s, out);
}

bool parseTermContinue(const char*& s, double current, double& out) {
	for (;;) {
		skipWs(s);
		if (*s != '*' && *s != '/') {
			break;
		}
		const char op = *s++;
		double rhs = 0.0;
		if (!parseFactor(s, current, rhs)) {
			return false;
		}
		if (op == '*') {
			out *= rhs;
		} else {
			if (rhs == 0.0) {
				return false;
			}
			out /= rhs;
		}
	}
	return true;
}

bool parseTerm(const char*& s, double current, double& out) {
	if (!parseFactor(s, current, out)) {
		return false;
	}
	return parseTermContinue(s, current, out);
}

bool parseExprContinue(const char*& s, double current, double& out) {
	for (;;) {
		skipWs(s);
		if (*s != '+' && *s != '-') {
			break;
		}
		const char op = *s++;
		double rhs = 0.0;
		if (!parseTerm(s, current, rhs)) {
			return false;
		}
		if (op == '+') {
			out += rhs;
		} else {
			out -= rhs;
		}
	}
	return true;
}

bool parseExpr(const char*& s, double current, double& out) {
	if (!parseTerm(s, current, out)) {
		return false;
	}
	return parseExprContinue(s, current, out);
}

} // namespace

bool evalNumericExpr(const char* buf, double current, double& out) {
	if (!buf) {
		return false;
	}
	const char* s = buf;
	skipWs(s);
	if (*s == '*' || *s == '/' || *s == '+') {
		out = current;
		if (*s == '*' || *s == '/') {
			if (!parseTermContinue(s, current, out) || !parseExprContinue(s, current, out)) {
				return false;
			}
		} else if (!parseExprContinue(s, current, out)) {
			return false;
		}
	} else if (!parseExpr(s, current, out)) {
		return false;
	}
	skipWs(s);
	return *s == '\0' && std::isfinite(out);
}

} // namespace rigkit
