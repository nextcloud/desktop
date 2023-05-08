/*
 * Copyright (C) 2024 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#pragma once
#include <openssl/evp.h>
#include <QtCore/qglobal.h>

namespace OCC
{
class Bio
{
public:
    Bio();

    ~Bio();

    operator const BIO *() const;
    operator BIO *();

private:
    Q_DISABLE_COPY(Bio)

    BIO *_bio;
};

class PKeyCtx
{
public:
    explicit PKeyCtx(int id, ENGINE *e = nullptr);

    ~PKeyCtx();

    // The move constructor is needed for pre-C++17 where
    // return-value optimization (RVO) is not obligatory
    // and we have a `forKey` static function that returns
    // an instance of this class
    PKeyCtx(PKeyCtx &&other);
    PKeyCtx &operator=(PKeyCtx &&other) = delete;

    static PKeyCtx forKey(EVP_PKEY *pkey, ENGINE *e = nullptr);

    operator EVP_PKEY_CTX *();

private:
    Q_DISABLE_COPY(PKeyCtx)

    PKeyCtx() = default;

    EVP_PKEY_CTX *_ctx = nullptr;
};

class PKey
{
public:
    ~PKey();

    // The move constructor is needed for pre-C++17 where
    // return-value optimization (RVO) is not obligatory
    // and we have a static functions that return
    // an instance of this class
    PKey(PKey &&other);

    PKey &operator=(PKey &&other) = delete;

    static PKey readPublicKey(Bio &bio);

    static PKey readPrivateKey(Bio &bio);

    static PKey generate(PKeyCtx &ctx);

    operator EVP_PKEY *();

    operator EVP_PKEY *() const;

private:
    Q_DISABLE_COPY(PKey)

    PKey() = default;

    EVP_PKEY *_pkey = nullptr;
};
}