#ifndef __CPPREGEX_H__
#define __CPPREGEX_H__

#include <regex>

class CPPRegex
{
	public:
		CPPRegex();
		int compile(const char *pat, int cflags);
                bool exec(const char *first, const char *last, std::cmatch& results);

	private:
                std::regex rx;
};

#endif // __CPPEGEX_H__
