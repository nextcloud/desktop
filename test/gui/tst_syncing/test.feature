Feature: Syncing files

    As a user
    I want to be able to sync my local folders to to my owncloud server
    so that I dont have to upload and download files manually

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    @smokeTest @issue-9281
    Scenario: Syncing a file to the server
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a file "lorem-for-upload.txt" with the following content inside the sync folder
            """
            test content
            """
        And the user waits for file "lorem-for-upload.txt" to be synced
        Then as "Alice" the file "lorem-for-upload.txt" should have the content "test content" in the server


    Scenario: Syncing all files and folders from the server
        Given user "Alice" has created folder "simple-folder" in the server
        And user "Alice" has created folder "large-folder" in the server
        And user "Alice" has uploaded file on the server with content "test content" to "uploaded-lorem.txt"
        And user "Alice" has set up a client with default settings
        Then the file "uploaded-lorem.txt" should exist on the file system
        And the file "uploaded-lorem.txt" should exist on the file system with the following content
            """
            test content
            """
        And the folder "simple-folder" should exist on the file system
        And the folder "large-folder" should exist on the file system

    @issue-9733
    Scenario: Syncing a file from the server and creating a conflict
        Given user "Alice" has uploaded file on the server with content "server content" to "/conflict.txt"
        And user "Alice" has set up a client with default settings
        And the user has paused the file sync
        And the user has changed the content of local file "conflict.txt" to:
            """
            client content
            """
        And user "Alice" has uploaded file on the server with content "changed server content" to "/conflict.txt"
        When the user resumes the file sync on the client
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        Then the table of conflict warnings should include file "conflict.txt"
        And the file "conflict.txt" should exist on the file system with the following content
            """
            changed server content
            """
        And a conflict file for "conflict.txt" should exist on the file system with the following content
            """
            client content
            """

    @skipOnOCIS
    Scenario: Sync all is selected by default
        Given user "Alice" has created folder "simple-folder" in the server
        And user "Alice" has created folder "large-folder" in the server
        And user "Alice" has uploaded file with content "test content" to "testFile.txt" in the server
        And user "Alice" has uploaded file with content "lorem content" to "lorem.txt" in the server
        And the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user sets the sync path in sync connection wizard
        And the user selects "ownCloud" as a remote destination folder
        Then the sync all checkbox should be checked
        When user unselects all the remote folders
        And the user adds the folder sync connection
        And the user waits for the files to sync
        Then the file "testFile.txt" should exist on the file system
        And the file "lorem.txt" should exist on the file system
        But the folder "simple-folder" should not exist on the file system
        And the folder "large-folder" should not exist on the file system

    @skipOnOCIS
    Scenario: Sync only one folder from the server
        Given user "Alice" has created folder "simple-folder" in the server
        And user "Alice" has created folder "large-folder" in the server
        And the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user sets the sync path in sync connection wizard
        And the user selects "ownCloud" as a remote destination folder
        And the user selects the following folders to sync:
            | folder        |
            | simple-folder |
        Then the folder "simple-folder" should exist on the file system
        But the folder "large-folder" should not exist on the file system
        When user "Alice" uploads file with content "some content" to "simple-folder/lorem.txt" in the server
        And user "Alice" uploads file with content "ownCloud" to "large-folder/lorem.txt" in the server
        And user "Alice" creates a file "simple-folder/localFile.txt" with the following content inside the sync folder
            """
            test content
            """
        And the user waits for the files to sync
        Then the file "simple-folder/lorem.txt" should exist on the file system
        And the file "large-folder/lorem.txt" should not exist on the file system
        And as "Alice" file "simple-folder/localFile.txt" should exist in the server
        When the user deletes the folder "simple-folder"
        And the user waits for the files to sync
        Then as "Alice" folder "simple-folder" should not exist in the server


    @issue-9733 @skipOnOCIS
    Scenario: sort folders list by name and size
        Given user "Alice" has created folder "123Folder" in the server
        And user "Alice" has uploaded file on the server with content "small" to "123Folder/lorem.txt"
        And user "Alice" has created folder "aFolder" in the server
        And user "Alice" has uploaded file on the server with content "more contents" to "aFolder/lorem.txt"
        And user "Alice" has created folder "bFolder" in the server
        And the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user sets the sync path in sync connection wizard
        And the user selects "ownCloud" as a remote destination folder
        # folders are sorted by name in ascending order by default
        Then the folders should be in the following order:
            | folder    |
            | 123Folder |
            | aFolder   |
            | bFolder   |
        # sort folder by name in descending order
        When the user sorts the folder list by "Name"
        Then the folders should be in the following order:
            | folder    |
            | bFolder   |
            | aFolder   |
            | 123Folder |
        # sort folder by size in ascending order
        When the user sorts the folder list by "Size"
        Then the folders should be in the following order:
            | folder    |
            | bFolder   |
            | 123Folder |
            | aFolder   |
        # sort folder by size in descending order
        When the user sorts the folder list by "Size"
        Then the folders should be in the following order:
            | folder    |
            | aFolder   |
            | 123Folder |
            | bFolder   |


    Scenario Outline: Syncing a folder to the server
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a folder <foldername> inside the sync folder
        And the user waits for folder <foldername> to be synced
        Then as "Alice" folder <foldername> should exist in the server
        Examples:
            | foldername                                                               |
            | "myFolder"                                                               |
            | "really long folder name with some spaces and special char such as $%ñ&" |
            | "folder with space at end "                                              |


    Scenario: Many subfolders can be synced
        Given user "Alice" has created folder "parent" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" creates a folder "parent/subfolderEmpty1" inside the sync folder
        And user "Alice" creates a folder "parent/subfolderEmpty2" inside the sync folder
        And user "Alice" creates a folder "parent/subfolderEmpty3" inside the sync folder
        And user "Alice" creates a folder "parent/subfolderEmpty4" inside the sync folder
        And user "Alice" creates a folder "parent/subfolderEmpty5" inside the sync folder
        And user "Alice" creates a folder "parent/subfolder1" inside the sync folder
        And user "Alice" creates a folder "parent/subfolder2" inside the sync folder
        And user "Alice" creates a folder "parent/subfolder3" inside the sync folder
        And user "Alice" creates a folder "parent/subfolder4" inside the sync folder
        And user "Alice" creates a folder "parent/subfolder5" inside the sync folder
        And user "Alice" creates a file "parent/subfolder1/test.txt" with the following content inside the sync folder
            """
            test content
            """
        And user "Alice" creates a file "parent/subfolder2/test.txt" with the following content inside the sync folder
            """
            test content
            """
        And user "Alice" creates a file "parent/subfolder3/test.txt" with the following content inside the sync folder
            """
            test content
            """
        And user "Alice" creates a file "parent/subfolder4/test.txt" with the following content inside the sync folder
            """
            test content
            """
        And user "Alice" creates a file "parent/subfolder5/test.txt" with the following content inside the sync folder
            """
            test content
            """
        And the user waits for the files to sync
        Then as "Alice" folder "parent/subfolderEmpty1" should exist in the server
        And as "Alice" folder "parent/subfolderEmpty2" should exist in the server
        And as "Alice" folder "parent/subfolderEmpty3" should exist in the server
        And as "Alice" folder "parent/subfolderEmpty4" should exist in the server
        And as "Alice" folder "parent/subfolderEmpty5" should exist in the server
        And as "Alice" folder "parent/subfolder1" should exist in the server
        And as "Alice" folder "parent/subfolder2" should exist in the server
        And as "Alice" folder "parent/subfolder3" should exist in the server
        And as "Alice" folder "parent/subfolder4" should exist in the server
        And as "Alice" folder "parent/subfolder5" should exist in the server


    Scenario: Both original and copied folders can be synced
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a folder "original" inside the sync folder
        And the user copies the folder "original" to "copied"
        And the user waits for folder "copied" to be synced
        Then as "Alice" folder "original" should exist in the server
        And as "Alice" folder "copied" should exist in the server

    @issue-9281
    Scenario: Verify that you can create a subfolder with long name
        Given user "Alice" has created folder "Folder1" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" creates a folder "Folder1/really long folder name with some spaces and special char such as $%ñ&" inside the sync folder
        And the user waits for folder "Folder1/really long folder name with some spaces and special char such as $%ñ&" to be synced
        Then the folder "Folder1/really long folder name with some spaces and special char such as $%ñ&" should exist on the file system
        And as "Alice" folder "Folder1/really long folder name with some spaces and special char such as $%ñ&" should exist in the server

    Scenario: Verify pre existing folders in local (Desktop client) are copied over to the server
        Given user "Alice" has created a folder "Folder1" inside the sync folder
        And user "Alice" has created a folder "Folder1/subFolder1" inside the sync folder
        And user "Alice" has created a folder "Folder1/subFolder1/subFolder2" inside the sync folder
        And user "Alice" has set up a client with default settings
        Then as "Alice" folder "Folder1" should exist in the server
        And as "Alice" folder "Folder1/subFolder1" should exist in the server
        And as "Alice" folder "Folder1/subFolder1/subFolder2" should exist in the server


    Scenario: Filenames that are rejected by the server are reported
        Given user "Alice" has created folder "Folder1" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" creates a file "Folder1/a\\a.txt" with the following content inside the sync folder
            """
            test content
            """
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        Then the file "Folder1/a\\a.txt" should exist on the file system
        And the file "Folder1/a\\a.txt" should be blacklisted


    Scenario Outline: Verify one empty folder with a length longer than the allowed limit will not be synced
        Given user "Alice" has created folder "<foldername>" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" creates a folder "<foldername>/<foldername>" inside the sync folder
        And user "Alice" creates a folder "<foldername>/<foldername>/<foldername>" inside the sync folder
        And user "Alice" creates a folder "<foldername>/<foldername>/<foldername>/<foldername>" inside the sync folder
        And user "Alice" creates a folder "<foldername>/<foldername>/<foldername>/<foldername>/<foldername>" inside the sync folder
        And the user waits for folder "<foldername>/<foldername>/<foldername>/<foldername>/<foldername>" to be synced
        Then as "Alice" folder "<foldername>/<foldername>" should exist in the server
        And as "Alice" folder "<foldername>/<foldername>/<foldername>" should exist in the server
        And as "Alice" folder "<foldername>/<foldername>/<foldername>/<foldername>" should exist in the server
        And as "Alice" folder "<foldername>/<foldername>/<foldername>/<foldername>/<foldername>" should exist in the server
        Examples:
            | foldername                                                      |
            | An empty folder which name is obviously more than 59 characters |


    Scenario: Invalid system names are synced in linux
        Given user "Alice" has created folder "CON" in the server
        And user "Alice" has created folder "test%" in the server
        And user "Alice" has uploaded file on the server with content "server content" to "/PRN"
        And user "Alice" has uploaded file on the server with content "server content" to "/foo%"
        And user "Alice" has set up a client with default settings
        Then the folder "CON" should exist on the file system
        And the folder "test%" should exist on the file system
        And the file "PRN" should exist on the file system
        And the file "foo%" should exist on the file system
        And as "Alice" folder "CON" should exist in the server
        And as "Alice" folder "test%" should exist in the server
        And as "Alice" file "/PRN" should exist in the server
        And as "Alice" file "/foo%" should exist in the server


    Scenario: various types of files can be synced from server to client
        Given user "Alice" has created folder "simple-folder" in the server
        And user "Alice" has uploaded file "testavatar.png" to "simple-folder/testavatar.png" on the server
        And user "Alice" has uploaded file "testavatar.jpg" to "simple-folder/testavatar.jpg" on the server
        And user "Alice" has uploaded file "testavatar.jpeg" to "simple-folder/testavatar.jpeg" on the server
        And user "Alice" has uploaded file "testimage.mp3" to "simple-folder/testimage.mp3" on the server
        And user "Alice" has uploaded file "test_video.mp4" to "simple-folder/test_video.mp4" on the server
        And user "Alice" has uploaded file "simple.pdf" to "simple-folder/simple.pdf" on the server
        And user "Alice" has set up a client with default settings
        Then the folder "simple-folder" should exist on the file system
        And the file "simple-folder/testavatar.png" should exist on the file system
        And the file "simple-folder/testavatar.jpg" should exist on the file system
        And the file "simple-folder/testavatar.jpeg" should exist on the file system
        And the file "simple-folder/testimage.mp3" should exist on the file system
        And the file "simple-folder/test_video.mp4" should exist on the file system
        And the file "simple-folder/simple.pdf" should exist on the file system


    Scenario: various types of files can be synced from client to server
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates the following files inside the sync folder:
            | files            |
            | /testavatar.png  |
            | /testavatar.jpg  |
            | /testavatar.jpeg |
            | /testaudio.mp3   |
            | /test_video.mp4  |
            | /simple.txt      |
        And the user waits for the files to sync
        Then as "Alice" file "testavatar.png" should exist in the server
        And as "Alice" file "testavatar.jpg" should exist in the server
        And as "Alice" file "testavatar.jpeg" should exist in the server
        And as "Alice" file "testaudio.mp3" should exist in the server
        And as "Alice" file "test_video.mp4" should exist in the server
        And as "Alice" file "simple.txt" should exist in the server


    Scenario Outline: File with long name can be synced
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a file "<filename>" with the following content inside the sync folder
            """
            test content
            """
        And the user waits for file "<filename>" to be synced
        Then as "Alice" file "<filename>" should exist in the server
        Examples:
            | filename                                                                                                                                                                                                                     |
            | thisIsAVeryLongFileNameToCheckThatItWorks-thisIsAVeryLongFileNameToCheckThatItWorks-thisIsAVeryLongFileNameToCheckThatItWorks-thisIsAVeryLongFileNameToCheckThatItWorks-thisIsAVeryLongFileNameToCheckThatItWorks-thisIs.txt |

    @skipOnOCIS @issue-11104 @skip
    Scenario Outline: File with long name (233 characters) is blacklisted
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a file "<filename>" with the following content inside the sync folder
            """
            test contents
            """
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        Then the file "<filename>" should be blacklisted
        Examples:
            | filename                                                                                                                                                                                                                                  |
            | aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFoxJumpsOverAVeryLazyDog-aQuickBrownFo.txt |


    Scenario: Syncing file of 1 GB size
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a file "newfile.txt" with size "1GB" inside the sync folder
        And the user waits for file "newfile.txt" to be synced
        Then as "Alice" file "newfile.txt" should exist in the server


    Scenario: File with spaces in the name can sync
        Given user "Alice" has set up a client with default settings
        When user "Alice" creates a file "file with space.txt" with the following content inside the sync folder
            """
            test contents
            """
        And the user waits for file "file with space.txt" to be synced
        Then as "Alice" file "file with space.txt" should exist in the server


    Scenario: Syncing folders each having 500 files
        Given the user has created a folder "folder1" in temp folder
        And the user has created "500" files each of size "1048576" bytes inside folder "folder1" in temp folder
        And the user has created a folder "folder2" in temp folder
        And the user has created "500" files each of size "1048576" bytes inside folder "folder2" in temp folder
        And user "Alice" has set up a client with default settings
        When user "Alice" moves folder "folder1" from the temp folder into the sync folder
        And user "Alice" moves folder "folder2" from the temp folder into the sync folder
        And the user waits for folder "folder1" to be synced
        And the user waits for folder "folder2" to be synced
        Then as "Alice" folder "folder1" should exist in the server
        And as user "Alice" folder "folder1" should contain "500" items in the server
        And as "Alice" folder "folder2" should exist in the server
        And as user "Alice" folder "folder2" should contain "500" items in the server


    Scenario: Skip sync folder configuration
        Given the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user cancels the sync connection wizard
        Then the account with displayname "Alice Hansen" and host "%local_server_hostname%" should be displayed
        And the sync folder should not be added


    Scenario: extract a zip file in the sync folder
        Given the user has created a zip file "archive.zip" with the following resources in the temp folder
            | resource  | type   | content    |
            | folder1   | folder |            |
            | folder2   | folder |            |
            | file1.txt | file   | Test file1 |
            | file2.txt | file   | Test file2 |
        And user "Alice" has set up a client with default settings
        When user "Alice" moves file "archive.zip" from the temp folder into the sync folder
        And user "Alice" unzips the zip file "archive.zip" inside the sync root
        And the user waits for the files to sync
        Then as "Alice" folder "folder1" should exist in the server
        And as "Alice" folder "folder2" should exist in the server
        And as "Alice" the file "file1.txt" should have the content "Test file1" in the server
        And as "Alice" the file "file2.txt" should have the content "Test file2" in the server


    @skipOnOCIS
    Scenario: sync remote folder to a local sync folder having special characters
        Given user "Alice" has created folder "~`!@#$^&()-_=+{[}];',)" in the server
        And user "Alice" has created folder "simple-folder" in the server
        And user "Alice" has created folder "test-folder" in the server
        And user "Alice" has created folder "test-folder/sub-folder1" in the server
        And user "Alice" has created folder "test-folder/sub-folder2" in the server
        And user "Alice" has created folder "~test%" in the server
        And the user has created a folder "~`!@#$^&()-_=+{[}];',)PRN%" in temp folder
        And the user has started the client
        And the user has entered the following account information:
            | server   | %local_server% |
            | user     | Alice          |
            | password | 1234           |
        When the user selects manual sync folder option in advanced section
        And the user sets the temp folder "~`!@#$^&()-_=+{[}];',)PRN%" as local sync path in sync connection wizard
        And the user selects "ownCloud" as a remote destination folder
        And the user selects the following folders to sync:
            | folder                  |
            | ~`!@#$^&()-_=+{[}];',)  |
            | simple-folder           |
            | test-folder/sub-folder2 |
        Then the folder "~`!@#$^&()-_=+{[}];',)" should exist on the file system
        And the folder "simple-folder" should exist on the file system
        But the folder "~test%" should not exist on the file system
        When user "Alice" deletes the folder "simple-folder" in the server
        And the user waits for the files to sync
        Then the folder "simple-folder" should not exist on the file system
        And the folder "test-folder/sub-folder2" should exist on the file system
        And the folder "test-folder/sub-folder1" should not exist on the file system
