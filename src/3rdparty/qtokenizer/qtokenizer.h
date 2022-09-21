/****************************************************************************
**
** Copyright (C) 2014 Daniel Molkentin <daniel@molkentin.de>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <QString>
#include <QByteArray>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE

template <class T, class const_iterator>
struct QTokenizerPrivate {
    typedef typename T::value_type char_type;

    struct State {
        bool inQuote;
        bool inEscape;
        char_type quoteChar;
        State()
            : inQuote(false)
            , inEscape(false)
            , quoteChar(QLatin1Char('\0'))
        {
        }
    };

    QTokenizerPrivate(const T& _string, const T& _delims) :
        string(_string)
      , begin(string.begin())
      , end(string.end())
      , tokenBegin(end)
      , tokenEnd(begin)
      , delimiters(_delims)
      , isDelim(false)
      , returnDelimiters(false)
      , returnQuotes(false)
    {
    }

    bool isDelimiter(char_type c) const {
        return delimiters.contains(c);
    }

    bool isQuote(char_type c) const {
        return quotes.contains(c);
    }

    // Returns true if a delimiter was not hit
    bool nextChar(State* state, char_type c) {
        if (state->inQuote) {
            if (state->inEscape) {
                state->inEscape = false;
            } else if (c == QLatin1Char('\\')) {
                state->inEscape = true;
            } else if (c == state->quoteChar) {
                state->inQuote = false;
            }
        } else {
            if (isDelimiter(c))
                return false;
            state->inQuote = isQuote(state->quoteChar = c);
        }
        return true;
    }

    T string;
    // ### copies begin and end for performance, premature optimization?
    const_iterator begin;
    const_iterator end;
    const_iterator tokenBegin;
    const_iterator tokenEnd;
    T delimiters;
    T quotes;
    bool isDelim;
    bool returnDelimiters;
    bool returnQuotes;
};

template <class T, class const_iterator>
class QTokenizer {
public:
    typedef typename T::value_type char_type;

    /*!
       \class QTokenizer
       \inmodule QtNetwork
       \brief QTokenizer tokenizes Strings on QString, QByteArray,
              std::string or std::wstring

       Example Usage:

       \code
         QString str = ...;
         QByteArrayTokenizer tokenizer(str, "; ");
         tokenizer.setQuoteCharacters("\"'");
         tokenizer.setReturnDelimiters(true);
         while (tokenizer.hasNext()) {
           QByteArray token = tokenizer.next();
           bool isDelimiter = tokenizer.isDelimiter();
           ...
         }
       \endcode

       \param string The string to tokenize
       \param delimiters A string containing delimiters

       \sa QStringTokenizer, QByteArrayTokenizer, StringTokenizer, WStringTokenizer
     */
    QTokenizer(const T& string, const T& delimiters)
        : d(new QTokenizerPrivate<T, const_iterator>(string, delimiters))
    { }

    /*!
       Whether or not to return delimiters as tokens
       \see setQuoteCharacters
     */
    void setReturnDelimiters(bool enable) { d->returnDelimiters = enable; }


    /*!
       Sets characters that are considered to start and end quotes.

       When between two characters considered a quote, delimiters will
       be ignored.

       When between quotes, blackslash characters will cause the QTokenizer
       to skip the next character.

       \param quotes Characters that delimit quotes.
     */
    void setQuoteCharacters(const T& quotes) { d->quotes = quotes; }


    /*!
       Whether or not to return delimiters as tokens
       \see setQuoteCharacters
     */
    void setReturnQuoteCharacters(bool enable) { d->returnQuotes = enable; }


    /*!
       Retrieve next token.

       Returns true if there are more tokens, false otherwise.

       \sa next()
     */
    bool hasNext()
    {
        typename QTokenizerPrivate<T, const_iterator>::State state;
        d->isDelim = false;
        for (;;) {
            d->tokenBegin = d->tokenEnd;
            if (d->tokenEnd == d->end)
                return false;
            d->tokenEnd++;
            if (d->nextChar(&state, *d->tokenBegin))
                break;
            if (d->returnDelimiters) {
                d->isDelim = true;
                return true;
            }
        }
        while (d->tokenEnd != d->end && d->nextChar(&state, *d->tokenEnd)) {
            d->tokenEnd++;
        }
        return true;
    }

    /*!
       Resets the tokenizer to the starting position.
     */
    void reset() {
        d->tokenEnd = d->begin;
    }

    /*!
       Returns true if the current token is a delimiter,
       if one more more delimiting characters have been set.
     */
    bool isDelimiter() const { return d->isDelim; }

    /*!
       Returns the current token.

       Use \c hasNext() to fetch the next token.
     */
    T next() const {
        int len = d->tokenEnd-d->tokenBegin;
        const_iterator tmpStart = d->tokenBegin;
        if (!d->returnQuotes && len > 1 && d->isQuote(*d->tokenBegin)) {
            tmpStart++;
            len -= 2;
        }
        return T(tmpStart, len);
    }

private:
    friend class QStringTokenizer;
    QSharedPointer<QTokenizerPrivate<T, const_iterator> > d;
};

class QStringTokenizer : public QTokenizer<QString, QString::const_iterator> {
public:
    QStringTokenizer(const QString &string, const QString &delim) :
        QTokenizer<QString, QString::const_iterator>(string, delim) {}
    /**
     * @brief Like \see next(), but returns a lightweight string reference
     * @return A reference to the token within the string
     */
    QStringRef stringRef() {
        int begin = d->tokenBegin-d->begin;
        int end = d->tokenEnd-d->tokenBegin;
        if (!d->returnQuotes && d->isQuote(*d->tokenBegin)) {
            begin++;
            end -= 2;
        }
        return QStringRef(&d->string, begin, end);
    }
};

typedef QTokenizer<QByteArray, QByteArray::const_iterator> QByteArrayTokenizer;
typedef QTokenizer<std::string, std::string::const_iterator> StringTokenizer;
typedef QTokenizer<std::wstring, std::wstring::const_iterator> WStringTokenizer;

QT_END_NAMESPACE

#endif // TOKENIZER_H

