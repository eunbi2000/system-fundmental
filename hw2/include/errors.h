//for error.c functions

extern int errors;
extern int warnings;
extern int dbflag;

void fatal(char *fmt, ...);
void error(char *fmt, ...);
void warning(char *fmt, ...);
void debug(char *fmt, ...);