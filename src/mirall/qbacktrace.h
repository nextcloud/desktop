
#include <execinfo.h>
#include <stdlib.h>

static QString qBacktrace( int levels = -1 )
{
    QString s;
    void* trace[256];
    int n = backtrace(trace, 256);
    char** strings = backtrace_symbols (trace, n);

    if ( levels != -1 )
        n = qMin( n, levels );
    s = QString::fromLatin1("[\n");

    for (int i = 0; i < n; ++i)
        s += QString::number(i) +
             QString::fromLatin1(": ") +
             QString::fromLatin1(strings[i]) + QString::fromLatin1("\n");
    s += QString::fromLatin1("]\n");
    free (strings);
    return s;
}

