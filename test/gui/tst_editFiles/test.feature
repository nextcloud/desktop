Feature: edit files

    As a user
    I want to be able to edit the file content
    So that I can modify and change file data


    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files


    Scenario: Modify orignal content of a file with special character
        Given user "Alice" has uploaded file with content "ownCloud test text file 0" to "S@mpleFile!With,$pecial?Characters.txt" on the server
        And user "Alice" has set up a client with default settings
        When the user overwrites the file "S@mpleFile!With,$pecial?Characters.txt" with content "overwrite ownCloud test text file"
        When the user waits for file "S@mpleFile!With,$pecial?Characters.txt" to be synced
        Then as "Alice" the file "S@mpleFile!With,$pecial?Characters.txt" on the server should have the content "overwrite ownCloud test text file"