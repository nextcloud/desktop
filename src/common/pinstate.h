/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2019 ownCloud GmbH
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef PINSTATE_H
#define PINSTATE_H

#include "ocsynclib.h"

#include <QObject>

namespace OCC {

OCSYNC_EXPORT Q_NAMESPACE

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
     * If a unpinned file becomes hydrated (such as due to an implicit hydration
     * where the user requested access to the file's data) its pin state changes
     * to Unspecified.
     */
    OnlineOnly = 2,

    /** The user hasn't made a decision. The client or platform may hydrate or
     * dehydrate as they see fit.
     *
     * New remote files in unspecified directories start unspecified, and
     * dehydrated (which is an arbitrary decision).
     */
    Unspecified = 3,

    /** The file will never be synced to the cloud.
     * 
     * Useful for ignored files to indicate to the OS the file will never be
     * synced
     */
    Excluded = 4,
};
Q_ENUM_NS(PinState)

/** A user-facing version of PinState.
 *
 * PinStates communicate availability intent for an item, but particular
 * situations can get complex: An AlwaysLocal folder can have OnlineOnly
 * files or directories.
 *
 * For users this is condensed to a few useful cases.
 *
 * Note that this is only about *intent*. The file could still be out of date,
 * or not have been synced for other reasons, like errors.
 *
 * NOTE: The numerical values and ordering of this enum are relevant.
 */
enum class VfsItemAvailability {
    /** The item and all its subitems are hydrated and pinned AlwaysLocal.
     *
     * This guarantees that all contents will be kept in sync.
     */
    AlwaysLocal = 0,

    /** The item and all its subitems are hydrated.
     *
     * This may change if the platform or client decide to dehydrate items
     * that have Unspecified pin state.
     *
     * A folder with no file contents will have this availability.
     */
    AllHydrated = 1,

    /** There are dehydrated and hydrated items.
     *
     * This would happen if a dehydration happens to a Unspecified item that
     * used to be hydrated.
     */
    Mixed = 2,

    /** There are only dehydrated items but the pin state isn't all OnlineOnly.
     */
    AllDehydrated = 3,

    /** The item and all its subitems are dehydrated and OnlineOnly.
     *
     * This guarantees that contents will not take up space.
     */
    OnlineOnly = 4,
};
Q_ENUM_NS(VfsItemAvailability)

}

#endif
