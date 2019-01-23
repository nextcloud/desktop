/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#ifndef PINSTATE_H
#define PINSTATE_H

#include "ocsynclib.h"

namespace OCC {

/** Determines whether items should be available locally permanently or not
 *
 * The idea is that files and folders can be marked with the user intent
 * on availability.
 *
 * The Inherited state is used for resetting a pin state to what its
 * parent path would do.
 *
 * The pin state of a directory usually only matters for the initial pin and
 * hydration state of new remote files. It's perfectly possible for a
 * AlwaysLocal directory to have only OnlineOnly items. (though setting pin
 * states is usually done recursively, so one'd need to set the folder to
 * pinned and then each contained item to unpinned)
 *
 * Note: This enum intentionally mimics CF_PIN_STATE of Windows cfapi.
 */
enum class PinState {
    /** The pin state is derived from the state of the parent folder.
     *
     * For example new remote files start out in this state, following
     * the state of their parent folder.
     *
     * This state is used purely for resetting pin states to their derived
     * value. The effective state for an item will never be "Inherited".
     */
    Inherited = 0,

    /** The file shall be available and up to date locally.
     *
     * Also known as "pinned". Pinned dehydrated files shall be hydrated
     * as soon as possible.
     */
    AlwaysLocal = 1,

    /** File shall be a dehydrated placeholder, filled on demand.
     *
     * Also known as "unpinned". Unpinned hydrated files shall be dehydrated
     * as soon as possible.
     *
     * If a unpinned file becomes hydrated its pin state changes to unspecified.
     */
    OnlineOnly = 2,

    /** The user hasn't made a decision. The client or platform may hydrate or
     * dehydrate as they see fit.
     *
     * New remote files in unspecified directories start unspecified, and
     * dehydrated (which is an arbitrary decision).
     */
    Unspecified = 3,
};

}

#endif
