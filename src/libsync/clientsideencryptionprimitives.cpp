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

#include <QLoggingCategory>

#include <openssl/pem.h>

namespace OCC
{

Q_LOGGING_CATEGORY(lcCseUtility, "nextcloud.sync.clientsideencryption.utility", QtInfoMsg)

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
    Q_ASSERT(ctx._ctx);
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

PKey PKey::readHardwarePublicKey(PKCS11_KEY *key)
{
    PKey result;
    result._pkey = PKCS11_get_public_key(key);
    return result;
}

PKey PKey::readPrivateKey(Bio &bio)
{
    PKey result;
    result._pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    return result;
}

PKey PKey::readHardwarePrivateKey(PKCS11_KEY *key)
{
    PKey result;
    result._pkey = PKCS11_get_private_key(key);
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

Pkcs11Context::Pkcs11Context(State initState)
    : _pkcsS11Ctx(initState == State::CreateContext ? PKCS11_CTX_new() : nullptr)
{
}

Pkcs11Context::Pkcs11Context(Pkcs11Context &&otherContext)
    : _pkcsS11Ctx(otherContext._pkcsS11Ctx)
{
    otherContext._pkcsS11Ctx = nullptr;
}

Pkcs11Context::~Pkcs11Context()
{
    if (_pkcsS11Ctx) {
        PKCS11_CTX_free(_pkcsS11Ctx);
        _pkcsS11Ctx = nullptr;
    }
}

Pkcs11Context &Pkcs11Context::operator=(Pkcs11Context &&otherContext)
{
    if (&otherContext != this) {
        if (_pkcsS11Ctx) {
            PKCS11_CTX_free(_pkcsS11Ctx);
            _pkcsS11Ctx = nullptr;
        }
        std::swap(_pkcsS11Ctx, otherContext._pkcsS11Ctx);
    }

    return *this;
}

void Pkcs11Context::clear()
{
    if (_pkcsS11Ctx) {
        PKCS11_CTX_free(_pkcsS11Ctx);
        _pkcsS11Ctx = nullptr;
    }
}

}
