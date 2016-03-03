When disk space is low the ownCloud Client will be unable to synchronize all files. This section describes its behavior in a low disk space situation as well as the options that influence it.

1. Synchronization of a folder aborts entirely if the remaining disk space falls below 50 MB. This threshold can be adjusted with the ``OWNCLOUD_CRITICAL_FREE_SPACE_BYTES`` environment variable.

2. Downloads that would reduce the free disk space below 250 MB will be skipped or aborted. The download will be retried regularly and other synchronization is unaffected. This threshold can be adjusted with the ``OWNCLOUD_FREE_SPACE_BYTES`` environment variable.
