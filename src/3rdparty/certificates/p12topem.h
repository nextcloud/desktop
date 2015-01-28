/*
 * Copyright (C) by Pierre MOREAU <p.moreau@agim.idshost.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef P12TOPEM_H
#define	P12TOPEM_H

/**
 * \file p12topem.h
 * \brief Static library to convert p12 to pem
 * \author Pierre MOREAU <p.moreau@agim.idshost.fr>
 * \version 1.0.0
 * \date 09 January 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/pkcs12.h>

using namespace std;

/**
 * \struct resultP12ToPem p12topem.h
 */
struct resultP12ToPem {
    bool ReturnCode;
    int ErrorCode;
    string Comment;
    string PrivateKey;
    string Certificate;
};

/**
 * \brief Return string from BIO SSL
 * \param BIO o PEM_write_BIO_...
 * \return string PEM
 */
string x509ToString(BIO *o);

/**
 * \brief Convert P12 to PEM
 * \param string p12File Path to P12 file
 * \param string p12Passwd Password to open P12 file
 * \return result (bool ReturnCode, Int ErrorCode, String Comment, String PrivateKey, String Certificate)
 */
resultP12ToPem p12ToPem(string p12File, string p12Passwd);

#endif	/* P12TOPEM_H */

