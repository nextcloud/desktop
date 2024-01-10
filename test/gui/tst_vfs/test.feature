@skipOnLinux
Feature: Enable/disable virtual file support

    As a user
    I want to enable virtual file support
    So that I can synchronize virtual files with local folder


    Scenario: Disable/Enable VFS
        Given user "Alice" has been created on the server with default attributes and without skeleton files
        And user "Alice" has uploaded file with content "ownCloud" to "testFile.txt" in the server
        And user "Alice" has created folder "folder1" in the server
        And user "Alice" has uploaded file with content "some contents" to "folder1/lorem.txt" in the server
        And user "Alice" has created folder "folder2" in the server
        And user "Alice" has uploaded file with content "content" to "folder2/lorem.txt" in the server
        And user "Alice" has set up a client with default settings
        Then the placeholder of file "testFile.txt" should exist on the file system
        And the placeholder of file "folder1/lorem.txt" should exist on the file system
        And the placeholder of file "folder2/lorem.txt" should exist on the file system
        And the "Choose what to sync" button should not be available
        When the user disables virtual file support
        Then the "Enable virtual file support..." button should be available
        And the file "testFile.txt" should be downloaded
        And the file "folder1/lorem.txt" should be downloaded
        And the file "folder2/lorem.txt" should be downloaded
        When user unselects a folder "folder1" in selective sync
        And the user waits for the files to sync
        Then the folder "folder1" should not exist on the file system
        And the file "folder2/lorem.txt" should exist on the file system
        When the user enables virtual file support
        Then the "Disable virtual file support..." button should be available
        And the placeholder of file "folder1/lorem.txt" should exist on the file system
        And the file "testFile.txt" should be downloaded
        And the file "folder2/lorem.txt" should be downloaded
