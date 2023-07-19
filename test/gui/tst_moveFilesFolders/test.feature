Feature: move file and folder

    As a user
    I want to be able to move a folder and a file
    So that I can organize my files and folders


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Alice" has created folder "folder1" in the server
        And user "Alice" has created folder "folder1/folder2" in the server
        And user "Alice" has created folder "folder1/folder2/folder3" in the server
        And user "Alice" has created folder "folder1/folder2/folder3/folder4" in the server
        And user "Alice" has created folder "folder1/folder2/folder3/folder4/folder5" in the server


    Scenario: Move folder and file from level 5 sub-folder to sync root
        Given user "Alice" has created folder "folder1/folder2/folder3/folder4/folder5/test-folder" in the server
        And user "Alice" has uploaded file with content "ownCloud" to "folder1/folder2/folder3/folder4/folder5/lorem.txt" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" moves file "folder1/folder2/folder3/folder4/folder5/lorem.txt" to "/" in the sync folder
        And user "Alice" moves folder "folder1/folder2/folder3/folder4/folder5/test-folder" to "/" in the sync folder
        And the user waits for the files to sync
        Then as "Alice" the file "lorem.txt" should have the content "ownCloud" in the server
        And as "Alice" folder "test-folder" should exist in the server
        And as "Alice" file "folder1/folder2/folder3/folder4/folder5/lorem.txt" should not exist in the server
        And as "Alice" folder "folder1/folder2/folder3/folder4/folder5/test-folder" should not exist in the server


    Scenario: Move two folders and a file down to the level 5 sub-folder
        And user "Alice" has created folder "test-folder1" in the server
        And user "Alice" has created folder "test-folder2" in the server
        And user "Alice" has uploaded file with content "ownCloud test" to "testFile.txt" in the server
        And user "Alice" has set up a client with default settings
        When user "Alice" moves folder "test-folder1" to "folder1/folder2/folder3/folder4/folder5" in the sync folder
        And user "Alice" moves folder "test-folder2" to "folder1/folder2/folder3/folder4/folder5" in the sync folder
        And user "Alice" moves file "testFile.txt" to "folder1/folder2/folder3/folder4/folder5" in the sync folder
        And the user waits for the files to sync
        Then as "Alice" file "folder1/folder2/folder3/folder4/folder5/testFile.txt" should exist in the server
        And as "Alice" folder "folder1/folder2/folder3/folder4/folder5/test-folder1" should exist in the server
        And as "Alice" folder "folder1/folder2/folder3/folder4/folder5/test-folder2" should exist in the server
        And as "Alice" file "testFile.txt" should not exist in the server
        And as "Alice" folder "test-folder1" should not exist in the server
        And as "Alice" folder "test-folder2" should not exist in the server


    Scenario: Rename a file and a folder
        Given user "Alice" has uploaded file with content "test file 1" to "textfile.txt" in the server
        And user "Alice" has set up a client with default settings
        When the user renames a file "textfile.txt" to "lorem.txt"
        And the user renames a folder "folder1" to "FOLDER"
        And the user waits for the files to sync
        Then as "Alice" file "lorem.txt" should exist in the server
        And as "Alice" folder "FOLDER" should exist in the server
        But as "Alice" file "textfile.txt" should not exist in the server
        And as "Alice" folder "folder1" should not exist in the server
