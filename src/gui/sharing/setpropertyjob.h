/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "updatesharejob.h"

#include <optional>

namespace OCC::Gui::Sharing
{

/**
 * @brief Sets or clears one piece of share configuration.
 *
 * Properties are server-registered, typed settings compatible with the
 * share's sources or recipients, such as a label, note, password, or
 * expiration date. The returned representation is applied to the supplied
 * Share object.
 */
class SetPropertyJob : public UpdateShareJob
{
public:
    /**
     * @brief Creates a request to update a property.
     *
     * A missing value sends JSON null and clears the property.
     *
     * @param propertyClass Registered property type class to update
     * @param value Serialized property value, or no value to clear it
     */
    explicit SetPropertyJob(AccountPtr account,
                            Share &share,
                            const QString &propertyClass,
                            const std::optional<QString> &value);
};

}
