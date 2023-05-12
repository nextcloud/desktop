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
