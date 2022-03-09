Bugfix: Don't publish upload if we can't finish the transaction in the client

When a file gets locked during an upload we aborted after the upload finished on the server.
Resulting in a divergence of the local and remote state which could lead to conflicts.

https://github.com/owncloud/enterprise/issues/5052
