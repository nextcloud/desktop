Feature: Sharing

    As a user
    I want to share files and folders with other users
    So that those users can access the files and folders


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    @smokeTest
    Scenario: simple sharing
        Given user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" has uploaded on the server file with content "ownCloud test text file 0" to "/textfile0.txt"
        And user "Alice" has set up a client with default settings
        When the user adds "Brian Murphy" as collaborator of resource "%client_sync_path%/textfile0.txt" with permissions "edit,share" using the client-UI
        Then user "Brian Murphy" should be listed in the collaborators list for file "%client_sync_path%/textfile0.txt" with permissions "edit,share" on the client-UI

    @issue-7459
    Scenario: Progress indicator should not be visible after unselecting the password protection checkbox while sharing through public link
        Given user "Alice" has uploaded on the server file with content "ownCloud test text file 0" to "/textfile0.txt"
        And user "Alice" has set up a client with default settings
        When the user opens the public links dialog of "%client_sync_path%/textfile0.txt" using the client-UI
        And the user toggles the password protection using the client-UI
        And the user toggles the password protection using the client-UI
        Then the password progress indicator should not be visible in the client-UI - expected to fail

    @issue-7423
    Scenario: unshare a reshared file
        Given the setting "shareapi_auto_accept_share" on the server of app "core" has been set to "no"
        And the administrator on the server has set the default folder for received shares to "Shares"
        And user "Alice" has created on the server folder "simple-folder"
        And user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Carol" has been created on the server with default attributes and without skeleton files
        And user "Alice" has shared folder "simple-folder" on the server with user "Brian"
        And user "Brian" has accepted the share "simple-folder" on the server offered by user "Alice"
        And user "Brian" has shared folder "Shares/simple-folder" on the server with user "Carol"
        And user "Brian" has set up a client with default settings
        And user "Alice" has updated the share permissions on the server for folder "/simple-folder" to "read" for user "Brian"
        When user "Brian" opens the sharing dialog of "%client_sync_path%/Shares/simple-folder" using the client-UI
        Then the error text "The file can not be shared because it was shared without sharing permission." should be displayed in the sharing dialog

    @smokeTest
    Scenario: simple sharing of a file by public link without password
        Given user "Alice" has uploaded on the server file with content "ownCloud test text file 0" to "/textfile0.txt"
        And user "Alice" has set up a client with default settings
        When the user creates a new public link for file "%client_sync_path%/textfile0.txt" without password using the client-UI
        Then as user "Alice" the file "textfile0.txt" should have a public link on the server
        And the public should be able to download the file "textfile0.txt" without password from the last created public link by "Alice" on the server


    Scenario: simple sharing of a file by public link with password
        Given user "Alice" has set up a client with default settings
        And user "Alice" has uploaded on the server file with content "ownCloud test text file 0" to "/textfile0.txt"
        When the user creates a new public link for file "%client_sync_path%/textfile0.txt" with password "pass123" using the client-UI
        Then as user "Alice" the file "textfile0.txt" should have a public link on the server
        And the public should be able to download the file "textfile0.txt" with password "pass123" from the last created public link by "Alice" on the server

    @issue-8733
    Scenario: user changes the expiration date of an already existing public link using webUI
        Given user "Alice" has uploaded on the server file with content "ownCloud test text file 0" to "/textfile0.txt"
        And user "Alice" has set up a client with default settings
        And user "Alice" has created a public link on the server with following settings
            | path       | textfile0.txt |
            | name       | Public link   |
            | expireDate | 2031-10-14    |
        When the user opens the public links dialog of "%client_sync_path%/textfile0.txt" using the client-UI
        And the user edits the public link named "Public link" of file "textfile0.txt" changing following
            | expireDate | 2038-07-21 |
        Then the fields of the last public link share response of user "Alice" should include on the server
            | expireDate | 2038-07-21 |

    @smokeTest
    Scenario: simple sharing of a folder by public link without password
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" has set up a client with default settings
        When the user creates a new public link with permissions "Download / View" for folder "%client_sync_path%/simple-folder" without password using the client-UI
        Then as user "Alice" the folder "simple-folder" should have a public link on the server
        And the public should be able to download the folder "lorem.txt" without password from the last created public link by "Alice" on the server


    Scenario: simple sharing of a folder by public link with password
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" has set up a client with default settings
        When the user creates a new public link with permissions "Download / View" for folder "%client_sync_path%/simple-folder" with password "pass123" using the client-UI
        Then as user "Alice" the folder "simple-folder" should have a public link on the server
        And the public should be able to download the folder "lorem.txt" with password "pass123" from the last created public link by "Alice" on the server

    @issue-8733
    Scenario: user changes the expiration date of an already existing public link for folder using client-UI
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" has set up a client with default settings
        And user "Alice" has created a public link on the server with following settings
            | path        | simple-folder                |
            | name        | Public link                  |
            | expireDate  | 2031-10-14                   |
            | permissions | read, update, create, delete |
        When the user opens the public links dialog of "%client_sync_path%/simple-folder" using the client-UI
        And the user edits the public link named "Public link" of file "simple-folder" changing following
            | expireDate | 2038-07-21 |
        Then the fields of the last public link share response of user "Alice" on the server should include
            | expireDate | 2038-07-21 |


    Scenario Outline: simple sharing of folder by public link with different roles
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" has set up a client with default settings
        When the user creates a new public link for folder "%client_sync_path%/simple-folder" using the client-UI with these details:
            | role | <role> |
        Then user "Alice" on the server should have a share with these details:
            | field       | value          |
            | share_type  | public_link    |
            | uid_owner   | Alice          |
            | permissions | <permissions>  |
            | path        | /simple-folder |
            | name        | Public link    |
        Examples:
            | role        | permissions                  |
            | Viewer      | read                         |
            | Editor      | read, update, create, delete |
            | Contributor | create                       |


    Scenario: sharing by public link with "Uploader" role
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" on the server has created file "simple-folder/lorem.txt"
        And user "Alice" has set up a client with default settings
        When the user creates a new public link for folder "%client_sync_path%/simple-folder" with "Contributor" using the client-UI
        Then user "Alice" on the server should have a share with these details:
            | field       | value          |
            | share_type  | public_link    |
            | uid_owner   | Alice          |
            | permissions | create         |
            | path        | /simple-folder |
            | name        | Public link    |
        And the public should not be able to download the file "lorem.txt" from the last created public link by "Alice" on the server


    Scenario Outline: change collaborator permissions of a file & folder
        Given user "Alice" has created on the server folder "simple-folder"
        And user "Alice" on the server has created file "lorem.txt"
        And user "Brian" has been created on the server with default attributes and without skeleton files
        And user "Alice" on the server has shared folder "simple-folder" with user "Brian" with "all" permissions
        And user "Alice" on the server has shared file "lorem.txt" with user "Brian" with "all" permissions
        And user "Alice" has set up a client with default settings
        When the user removes permissions "<permissions>" for user "Brian Murphy" of resource "%client_sync_path%/simple-folder" using the client-UI
        And the user closes the sharing dialog
        And the user removes permissions "<permissions>" for user "Brian Murphy" of resource "%client_sync_path%/lorem.txt" using the client-UI
        Then "<permissions>" permissions should not be displayed for user "Brian Murphy" for resource "%client_sync_path%/simple-folder" on the client-UI
        And "<permissions>" permissions should not be displayed for user "Brian Murphy" for resource "%client_sync_path%/lorem.txt" on the client-UI
        And user "Alice" on the server should have a share with these details:
            | field       | value                        |
            | uid_owner   | Alice                        |
            | share_with  | Brian                        |
            | share_type  | user                         |
            | file_target | /Shares/simple-folder        |
            | item_type   | folder                       |
            | permissions | <expected-folder-permission> |
        And user "Alice" on the server should have a share with these details:
            | field       | value                      |
            | uid_owner   | Alice                      |
            | share_with  | Brian                      |
            | share_type  | user                       |
            | file_target | /Shares/lorem.txt          |
            | item_type   | file                       |
            | permissions | <expected-file-permission> |
        Examples:
            | permissions | expected-folder-permission   | expected-file-permission |
            | edit        | read, share                  | read, share              |
            | share       | read, update, create, delete | read,update              |
            | edit,share  | read                         | read                     |



