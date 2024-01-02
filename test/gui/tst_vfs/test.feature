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
        And user "Alice" has set up a client with default settings
        Then the placeholder of file "testFile.txt" should exist on the file system
        And the placeholder of file "folder1/lorem.txt" should exist on the file system
        When the user disables virtual file support
        Then the "Enable virtual file support..." button should be available
        And the file "testFile.txt" should be downloaded
        And the file "folder1/lorem.txt" should be downloaded
        And the file "testFile.txt" should exist on the file system with the following content
            """
            ownCloud
            """
        And the file "folder1/lorem.txt" should exist on the file system with the following content
            """
            some contents
            """
        When the user enables virtual file support
        Then the "Disable virtual file support..." button should be available
