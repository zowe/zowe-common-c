#ifndef __WINREGEX_H__
#define __WINREGEX_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* regcomp() cflags */

#define REG_EXTENDED       0x001   /* Use Extended RE syntax rules */
#define REG_ICASE          0x002   /* Ignore case in match         */
#define REG_NEWLINE        0x004   /* Convert <backslash><n> to
                                                           <newline> */
#define REG_NOSUB          0x008   /* regexec() not report
                                                   subexpressions */


/* regexec() eflags */

#define REG_NOTBOL  0x100   /* First character not start of line   */
#define REG_NOTEOL  0x200   /* Last character not end of line      */


/* Regular Expression error codes */

#define REG_NOMATCH        1  /* RE pattern not found           */
#define REG_BADPAT         2  /* Invalid Regular Expression     */
#define REG_ECOLLATE       3  /* Invalid collating element      */
#define REG_ECTYPE         4  /* Invalid character class        */
#define REG_EESCAPE        5  /* Last character is \            */
#define REG_ESUBREG        6  /* Invalid number in \digit       */
#define REG_EBRACK         7  /* imbalance                      */
#define REG_EPAREN         8  /* \( \) or () imbalance          */
#define REG_EBRACE         9  /* \{ \} or { } imbalance         */
#define REG_BADBR         10  /* Invalid \{ \} range exp        */
#define REG_ERANGE        11  /* Invalid range exp endpoint     */
#define REG_ESPACE        12  /* Out of memory                  */
#define REG_BADRPT        13  /* ?*+ not preceded by valid RE   */
#define REG_ECHAR         14  /* invalid multibyte character    */
#define REG_EBOL          15  /* ^ anchor and not BOL           */
#define REG_EEOL          16  /* $ anchor and not EOL           */
#define REG_EMPTY         17
#define REG_E_MEMORY      15
#define REG_E_UNKNOWN     21

  /*
  static const reg_error_t REG_BADPAT = 2;    // Invalid pattern.  
  static const reg_error_t REG_BADPAT = 2;    // Invalid pattern.  
  static const reg_error_t REG_ECOLLATE = 3;  // Undefined collating element.  
  static const reg_error_t REG_ECTYPE = 4;    // Invalid character class name. 
  static const reg_error_t REG_EESCAPE = 5;   // Trailing backslash.  
  static const reg_error_t REG_ESUBREG = 6;   // Invalid back reference.
  static const reg_error_t REG_EBRACK = 7;    // Unmatched left bracket.
  static const reg_error_t REG_EPAREN = 8;    // Parenthesis imbalance. 
  static const reg_error_t REG_EBRACE = 9;    // Unmatched \{.  
  static const reg_error_t REG_BADBR = 10;    // Invalid contents of \{\}. 
  static const reg_error_t REG_ERANGE = 11;   // Invalid range end. 
  static const reg_error_t REG_ESPACE = 12;   // Ran out of memory.  
  static const reg_error_t REG_BADRPT = 13;   // No preceding re for repetition op. 
  static const reg_error_t REG_EEND = 14;     // unexpected end of expression 
  static const reg_error_t REG_ESIZE = 15;    // expression too big 
  static const reg_error_t REG_ERPAREN = 8;   // = REG_EPAREN : unmatched right parenthesis 
  static const reg_error_t REG_EMPTY = 17;    // empty expression 
  static const reg_error_t REG_E_MEMORY = 15; // = REG_ESIZE : out of memory 
  static const reg_error_t REG_ECOMPLEXITY = 18; // complexity too high 
  static const reg_error_t REG_ESTACK = 19;   // out of stack space 
  static const reg_error_t REG_E_PERL = 20;   // Perl (?...) error 
  static const reg_error_t REG_E_UNKNOWN = 21; // unknown error 
  static const reg_error_t REG_ENOSYS = 21;   // = REG_E_UNKNOWN : Reserved.
    */

  
typedef struct winregex {
  void *obj;
} regex_t;

typedef int64_t regoff_t;

typedef struct regmatch{
  regoff_t rm_so; /* should be regoff_t */
  regoff_t rm_eo;
} regmatch_t;

regex_t *regexAlloc();
void regexFree(regex_t *m);

int regexComp(regex_t *r, const char *pattern, int cflags);
int regexExec(regex_t *r, const char *str, size_t nmatch,
              regmatch_t *pmatch, int eflags);

#ifdef __cplusplus
}
#endif

#endif /* __WINREGEX_H__ */
