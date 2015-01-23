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

/**
 * \file p12topem.cpp
 * \brief Static library to convert p12 to pem
 * \author Pierre MOREAU <p.moreau@agim.idshost.fr>
 * \version 1.0.0
 * \date 09 January 2014
 */

#include "p12topem.h"

/**
 * \fn string x509ToString (BIO)
 * \brief Return string from BIO SSL
 * \param BIO o PEM_write_BIO_...
 * \return string PEM
 */
string x509ToString(BIO *o) {
    int len = 0;
    BUF_MEM *bptr;
    void* data;
    string ret = "";
    
    BIO_get_mem_ptr(o, &bptr);
    len = bptr->length;
    data = calloc(len+10, sizeof(char));
    BIO_read(o, data, len);
    ret = strdup((char*)data);
    free(data);
        
    return ret;
}

/**
 * \fn resultP12ToPem p12ToPem (string, string)
 * \brief Convert P12 to PEM
 * \param string p12File Path to P12 file
 * \param string p12Passwd Password to open P12 file
 * \return result (bool ReturnCode, Int ErrorCode, String Comment, String PrivateKey, String Certificate)
 */
resultP12ToPem p12ToPem(string p12File, string p12Passwd) {
    FILE *fp;
    PKCS12 *p12 = NULL;
    EVP_PKEY *pkey = NULL;
    X509 *cert = NULL;
    STACK_OF(X509) *ca = NULL;
    
    BIO *o = BIO_new(BIO_s_mem());
    
    string privateKey = "";
    string certificate = "";
        
    resultP12ToPem ret;
    ret.ReturnCode = false;
    ret.ErrorCode = 0;
    ret.Comment = "";
    ret.PrivateKey = "";
    ret.Certificate = "";
    
    SSLeay_add_all_algorithms();
    ERR_load_crypto_strings();
    if(!(fp = fopen(p12File.c_str(), "rb"))) {
        ret.ErrorCode = 1;
        ret.Comment = strerror(errno);
        return ret;
    }
    
    p12 = d2i_PKCS12_fp(fp, &p12);
    fclose (fp);
    
    if (!p12) {
        ret.ErrorCode = 2;
        ret.Comment = "Unable to open PKCS#12 file";
        return ret;
    }
    if (!PKCS12_parse(p12, p12Passwd.c_str(), &pkey, &cert, &ca)) {
        ret.ErrorCode = 3;
        ret.Comment = "Unable to parse PKCS#12 file (wrong password ?)";
        return ret;
    }
    PKCS12_free(p12);
    
    if (!(pkey && cert)) {
        ret.ErrorCode = 4;
        ret.Comment = "Certificate and/or key file doesn't exists";
    } else {
        PEM_write_bio_PrivateKey(o, pkey, 0, 0, 0, NULL, 0);
        privateKey = x509ToString(o);
                
        PEM_write_bio_X509(o, cert);
        certificate = x509ToString(o);
        
        BIO_free(o);
        
        ret.ReturnCode = true;
        ret.ErrorCode = 0;
        ret.Comment = "All is fine";
        ret.PrivateKey = privateKey;
        ret.Certificate = certificate;
    }
    return ret;
}
