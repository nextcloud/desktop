<!--
Thanks for reporting issues back to Nextcloud!

Note: This is the **issue tracker of Nextcloud**, please do NOT use this to get answers to your questions or get help for fixing your installation. This is a place to report bugs to developers, after your server has been debugged. You can find help debugging your system on our home user forums: https://help.nextcloud.com or, if you use Nextcloud in a large organization, ask our engineers on https://portal.nextcloud.com. See also  https://nextcloud.com/support for support options.

Nextcloud is an open source project backed by Nextcloud GmbH. Most of our volunteers are home users and thus primarily care about issues that affect home users. Our paid engineers prioritize issues of our customers. If you are neither a home user nor a customer, consider paying somebody to fix your issue, do it yourself or become a customer.

Please understand that at the moment, we are very busy with customer issues and some high priority development work. A lot of issues are getting reported. We can't always keep up and timely respond to all of them, but we try!

Guidelines for submitting issues:

* Please search the existing issues first, it's likely that your issue was already reported or even fixed.
    - Go to https://github.com/nextcloud and type any word in the top search/command bar. You probably see something like "We couldn’t find any repositories matching ..." then click "Issues" in the left navigation.
    - You can also filter by appending e. g. "state:open" to the search string.
    - More info on search syntax within github: https://help.github.com/articles/searching-issues
    
* Please fill in as much of the template below as possible. We know it is a pain sometimes, but especially without logs there is often not much we can do: really, if we would have seen the issue you encoutered before, we would already have fixed it. So we did NOT see it, and we will need YOUR help to find out what is wrong and fix it. The logs are absoutely crucial for that. Expect us to quickly close issues without logs or other information we need. If you don't have time to gather the required information, we don't either.

Please also note that we have a https://nextcloud.com/contribute/code-of-conduct/ that applies on Github. To summarize it: please, be kind. We try our best to be nice, too. If you can't be bothered to be polite, please just don't bother to report issues as we won't feel motivated to help you. Remember, we don't get paid to help you!
-->

<!--- Please keep the note below for others who read your bug report -->

### How to use GitHub

* Please use the 👍 [reaction](https://blog.github.com/2016-03-10-add-reactions-to-pull-requests-issues-and-comments/) to show that you are affected by the same issue.
* Please don't comment if you have no relevant information to add. It's just extra noise for everyone subscribed to this issue.
* Subscribe to receive notifications on status change and new comments. 


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

