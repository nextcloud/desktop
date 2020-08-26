<!--
Dear user, please understand that at the moment, we are very busy with customer issues and some high priority development work. 
A lot of issues are getting reported. We can't always keep up and timely respond to all of them. 
Don't forget that Github is not a support system. If you require support for your business use of Nextcloud, see nextcloud.com/support for support options!
We only respond to issues that are following this template as much as possible - expect us to quickly close issues without logs or other information we need.
Please also note that we have a https://nextcloud.com/contribute/code-of-conduct/ that applies on Github. 
For questions try our forums: https://help.nextcloud.com
-->

```
If you have the same issue or agree with somebody's comment, please do not add "me too" style comments. This is not helpful and makes it harder for our developers to follow the conversation. Just use the Github reactions emojis to like, agree with or disagree with comments.
```

### Expected behaviour
Tell us what should happen

### Actual behaviour
Tell us what happens instead
<!--
Did you try end-to-end encryption before version 3.0? Following the instructions from this post might solve your problem since you might need to clean up the keys as that can break the functioning of >3.0 if you had a malformed key: https://help.nextcloud.com/t/help-test-the-latest-version-of-e2ee/87590
-->

### Steps to reproduce
1.
2.
3.

### Client configuration
Client version:
<!---
Please try to only report a bug if it happens with the latest version
The latest version can be seen by checking https://nextcloud.com/install/#install-clients
In the case of end-to-end encryption bug reports the client must be at least 3.0 and the server at least 19 with the end to end encryption app version at least 1.5.2.
--->

Operating system:

OS language:

Qt version used by client package (Linux only, see also Settings dialog):

Client package (From Nextcloud or distro) (Linux only):

Installation path of client:


### Server configuration
<!---
Optional section. It depends on the issue.
--->
Nextcloud version:

Storage backend (external storage):

### Logs

Please use Gist (https://gist.github.com/) or a similar code paster for longer
logs.

1. Client logfile: 
<!-- desktop client logs are a hard requirement for bug reports because we don't know how to do magic here :) -->
Output of `nextcloud --logdebug --logwindow` or `nextcloud --logdebug --logfile log.txt`
(On Windows using `cmd.exe`, you might need to first `cd` into the Nextcloud directory)
(See also https://docs.nextcloud.com/desktop/2.3/troubleshooting.html#log-files)

2. Web server error log:

3. Server logfile: nextcloud log (data/nextcloud.log):

