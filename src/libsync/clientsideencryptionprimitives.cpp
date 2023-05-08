/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include "clientsideencryptionprimitives.h"
#include <openssl/pem.h>

namespace OCC
{
Bio::Bio()
    : _bio(BIO_new(BIO_s_mem()))
{
}
Bio::~Bio()
{
    BIO_free_all(_bio);
}
Bio::operator const BIO *() const
{
    return _bio;
}

Bio::operator BIO *()
{
    return _bio;
}

PKeyCtx::PKeyCtx(int id, ENGINE *e)
    : _ctx(EVP_PKEY_CTX_new_id(id, e))
{
}

PKeyCtx::PKeyCtx(PKeyCtx &&other)
{
    std::swap(_ctx, other._ctx);
}

PKeyCtx::~PKeyCtx()
{
    EVP_PKEY_CTX_free(_ctx);
}

PKeyCtx PKeyCtx::forKey(EVP_PKEY *pkey, ENGINE *e)
{
    PKeyCtx ctx;
    ctx._ctx = EVP_PKEY_CTX_new(pkey, e);
    return ctx;
}

PKeyCtx::operator EVP_PKEY_CTX *()
{
    return _ctx;
}

PKey::~PKey()
{
    EVP_PKEY_free(_pkey);
}

PKey::PKey(PKey &&other)
{
    std::swap(_pkey, other._pkey);
}

PKey PKey::readPublicKey(Bio &bio)
{
    PKey result;
    result._pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    return result;
}

PKey PKey::readPrivateKey(Bio &bio)
{
    PKey result;
    result._pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    return result;
}

PKey PKey::generate(PKeyCtx &ctx)
{
    PKey result;
    if (EVP_PKEY_keygen(ctx, &result._pkey) <= 0) {
        result._pkey = nullptr;
    }
    return result;
}

PKey::operator EVP_PKEY *()
{
    return _pkey;
}

PKey::operator EVP_PKEY *() const
{
    return _pkey;
}

}