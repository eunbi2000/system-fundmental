/*
 * Error handling routines
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "errors.h"

int errors;
int warnings;
int dbflag = 1;

void fatal(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "\nFatal error: ");
        vprintf(fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
}

void error(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "\nError: ");
        vprintf(fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        errors++;
}

void warning(char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "\nWarning: ");
        vprintf(fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
        warnings++;
}

void debug(char *fmt, ...)
{
        if(!dbflag) return;
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "\nDebug: ");
        vprintf(fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
}
