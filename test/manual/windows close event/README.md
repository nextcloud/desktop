# Simple application to simulate application termination by MSI

This is a simple application that sends the Window messages `WM_QUERYENDSESSION` and `WM_ENDSESSION` to all windows of an application.
Those messages are sent by MSI when a current running application should quit.
Qt 5 handles those window messages incorrect causing our client to close, with this application the behaviour can be validated.

## Use
```
close_window.exe owncloud.exe
```
If invoked without an argument it will try to close owncloud.exe
