/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest

import Style
import "qrc:/qml/src/gui/assistant/qml" as Assistant

Item {
    id: testRoot

    width: 800
    height: 640

    Component {
        id: assistantWindowComponent

        Assistant.AssistantWindow {}
    }

    Component {
        id: chatViewComponent

        Assistant.AssistantChatView {
            width: 700
            height: 500
        }
    }

    Component {
        id: taskViewComponent

        Assistant.AssistantTaskView {
            width: 700
            height: 500
        }
    }

    Component {
        id: conversationDelegateComponent

        Assistant.AssistantConversationDelegate {
            index: 1
            title: "Conversation"
            selected: true
            pickerFont: Qt.font({ pixelSize: 14 })
            pickerHighlightedIndex: 1
            pickerWidth: 400
        }
    }

    Component {
        id: messageDelegateComponent

        Assistant.AssistantMessageDelegate {
            messageRole: "user"
            messageText: "Question"
            dateText: "Today"
        }
    }

    Component {
        id: taskTypeSelectorComponent

        Assistant.AssistantTaskTypeSelector {
            width: 400
            assistantController: assistantTestSetup.controller
            canUseAssistant: true
        }
    }

    TestCase {
        name: "AssistantQml"
        when: windowShown

        property var createdObject: null

        function init() {
            assistantTestSetup.reset()
        }

        function cleanup() {
            if (createdObject) {
                createdObject.destroy()
                createdObject = null
            }
        }

        function createAssistantWindow() {
            createdObject = assistantWindowComponent.createObject(null, {
                accountName: "Alice",
                accountServer: "cloud.example.test",
                accountAvatar: "",
                assistantController: assistantTestSetup.controller,
                visible: true
            })
            verify(createdObject !== null)
            return createdObject
        }

        function createChatView() {
            createdObject = chatViewComponent.createObject(testRoot, {
                assistantController: assistantTestSetup.controller
            })
            verify(createdObject !== null)
            return createdObject
        }

        function createTaskView() {
            createdObject = taskViewComponent.createObject(testRoot, {
                assistantController: assistantTestSetup.controller
            })
            verify(createdObject !== null)
            return createdObject
        }

        function test_windowBlocksSubmissionWithoutSupportedTaskType() {
            const window = createAssistantWindow()
            assistantTestSetup.completeEmptyTaskTypes()
            const input = findChild(window, "assistantQuestionInput")
            const sendButton = findChild(window, "assistantSendButton")

            verify(input !== null)
            verify(sendButton !== null)
            input.text = "Question"
            compare(assistantTestSetup.controller.selectedTaskTypeId, "")
            compare(sendButton.enabled, false)
        }

        function test_windowLoadsChatComponentsAndMessages() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.selectConversationAndCompleteMessages()

            const selector = findChild(window, "assistantTaskTypeSelector")
            const chatView = findChild(window, "assistantChatView")
            const picker = findChild(window, "assistantConversationPicker")
            const messageList = findChild(window, "assistantMessageList")

            verify(selector !== null)
            compare(selector.visible, false)
            verify(chatView !== null)
            verify(picker !== null)
            compare(picker.count, 1)
            verify(messageList !== null)
            tryCompare(messageList, "count", 1)
            verify(messageList.itemAtIndex(0) !== null)
            compare(messageList.itemAtIndex(0).objectName, "assistantMessageDelegate")
        }

        function test_chatViewStartsNewConversation() {
            assistantTestSetup.controller.loadData()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.selectConversationAndCompleteMessages()
            const chatView = createChatView()
            const newConversationButton = findChild(chatView, "assistantNewConversationButton")
            const messageList = findChild(chatView, "assistantMessageList")

            verify(newConversationButton !== null)
            compare(messageList.count, 1)
            mouseClick(newConversationButton)
            compare(assistantTestSetup.controller.selectedChatConversationId, -1)
            compare(messageList.count, 0)
        }

        function test_taskViewRetriesTask() {
            assistantTestSetup.seedTask()
            const taskView = createTaskView()
            const taskList = findChild(taskView, "assistantTaskList")

            verify(taskList !== null)
            tryCompare(taskList, "count", 1)
            const delegate = taskList.itemAtIndex(0)
            verify(delegate !== null)
            compare(delegate.objectName, "assistantTaskDelegate")
            const retryButton = findChild(delegate, "assistantRetryTaskButton")
            verify(retryButton !== null)
            mouseClick(retryButton)
            compare(assistantTestSetup.scheduleTaskCount, 1)
        }

        function test_taskViewConfirmsDeletion() {
            assistantTestSetup.seedTask()
            const taskView = createTaskView()
            const taskList = findChild(taskView, "assistantTaskList")

            tryCompare(taskList, "count", 1)
            const deleteButton = findChild(taskList.itemAtIndex(0), "assistantDeleteTaskButton")
            verify(deleteButton !== null)
            mouseClick(deleteButton)

            const dialog = findChild(taskView, "assistantDeleteTaskDialog")
            verify(dialog !== null)
            tryCompare(dialog, "visible", true)
            const confirmButton = findChild(dialog, "assistantDeleteTaskConfirmButton")
            verify(confirmButton !== null)
            mouseClick(confirmButton)
            compare(assistantTestSetup.deleteTaskCount, 1)
            compare(assistantTestSetup.lastDeletedTaskId, 7)
        }

        function test_standaloneDelegatesExposeTheirState() {
            createdObject = conversationDelegateComponent.createObject(testRoot)
            verify(createdObject !== null)
            compare(createdObject.objectName, "assistantConversationDelegate")
            compare(createdObject.highlighted, true)
            compare(createdObject.selected, true)
            createdObject.destroy()

            createdObject = messageDelegateComponent.createObject(testRoot)
            verify(createdObject !== null)
            compare(createdObject.objectName, "assistantMessageDelegate")
            compare(createdObject.isAssistantMessage, false)
            createdObject.destroy()

            createdObject = taskTypeSelectorComponent.createObject(testRoot)
            verify(createdObject !== null)
            compare(createdObject.objectName, "assistantTaskTypeSelector")
            compare(createdObject.visible, false)
        }
    }
}
