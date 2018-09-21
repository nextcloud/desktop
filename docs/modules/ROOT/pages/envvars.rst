The behavior of the client can also be controlled using environment variables. The value of the environment variables overrides the values in the configuration file.

The environment variables are:

- `OWNCLOUD_CHUNK_SIZE` (default: 5242880; 5 MiB) – Specifies the chunk size of uploaded files in bytes. Increasing this value may help with synchronization problems in certain configurations.  
- `OWNCLOUD_TIMEOUT` (default: 300 s) – The timeout for network connections in seconds.
- `OWNCLOUD_CRITICAL_FREE_SPACE_BYTES` (default: 50\*1000\*1000 bytes) - The minimum disk space needed for operation. A fatal error is raised if less free space is available. 
- `OWNCLOUD_FREE_SPACE_BYTES` (default: 250\*1000\*1000 bytes) - Downloads that would reduce the free space below this value are skipped. More information available under the "Low Disk Space" section. 
- `OWNCLOUD_MAX_PARALLEL` (default: 6) - Maximum number of parallel jobs. 
- `OWNCLOUD_BLACKLIST_TIME_MIN` (default: 25 s) - Minimum timeout for blacklisted files.
- `OWNCLOUD_BLACKLIST_TIME_MAX` (default: 24\*60\*60 s; one day) - Maximum timeout for blacklisted files.
