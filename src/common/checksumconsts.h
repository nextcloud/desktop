/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

namespace OCC
{
/**
 * Tags for checksum headers values.
 * They are here for being shared between Upload- and Download Job
 */
static constexpr auto checkSumMD5C = "MD5";
static constexpr auto checkSumSHA1C = "SHA1";
static constexpr auto checkSumSHA2C = "SHA256";
static constexpr auto checkSumSHA3C = "SHA3-256";
static constexpr auto checkSumAdlerC = "Adler32";
}
