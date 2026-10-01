/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest

import Style
import "qrc:/qml/src/gui/assistant/qml" as Assistant
import "qrc:/qml/src/gui/wizard/qml" as Wizard

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
        id: wizardButtonComponent

        Wizard.WizardButton {
            width: Style.wizardFooterButtonHeight
            iconSource: "qrc:/client/theme/add.svg"
            leftPadding: 0
            rightPadding: 0
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

    Component {
        id: taskTypeDelegateComponent

        Assistant.AssistantTaskTypeDelegate {
            assistantController: assistantTestSetup.controller
            canUseAssistant: true
            typeId: "core:text2text:summarize"
            name: "Summarize"
            isChat: false
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

        function buttonIcon(button) {
            if (!button.iconBeforeText) {
                return findChild(button, "wizardButtonFollowingIcon")
            }
            return findChild(button, button.tintIcon ? "wizardButtonLeadingIconTint" : "wizardButtonLeadingIcon")
        }

        function verifyIconCentered(button) {
            waitForPolish(button.contentItem)
            const icon = buttonIcon(button)
            verify(icon !== null)
            verify(icon.visible)
            const center = icon.mapToItem(button, icon.width / 2, icon.height / 2)
            fuzzyCompare(center.x, button.width / 2, 1)
            fuzzyCompare(center.y, button.height / 2, 1)
        }

        function test_windowHeadlineIsBrandNeutral() {
            const window = createAssistantWindow()
            compare(window.headline, "Assistant")
            verify(window.headline.indexOf("Nextcloud") === -1)
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

        function test_chatActionIconsAreCentered() {
            const chatView = createChatView()
            waitForPolish(chatView)
            for (const buttonName of ["assistantNewConversationButton", "assistantReloadConversationsButton"]) {
                const button = findChild(chatView, buttonName)
                verify(button !== null)
                verifyIconCentered(button)
            }
        }

        function test_iconOnlyWizardButtonIsCentered_data() {
            return [
                { tag: "leading", before: true, tinted: false, enabled: true },
                { tag: "leadingDisabled", before: true, tinted: false, enabled: false },
                { tag: "leadingTinted", before: true, tinted: true, enabled: true },
                { tag: "leadingTintedDisabled", before: true, tinted: true, enabled: false },
                { tag: "following", before: false, tinted: false, enabled: true },
                { tag: "followingDisabled", before: false, tinted: false, enabled: false }
            ]
        }

        function test_iconOnlyWizardButtonIsCentered(data) {
            const button = createTemporaryObject(wizardButtonComponent, testRoot, {
                iconBeforeText: data.before,
                tintIcon: data.tinted,
                enabled: data.enabled
            })
            verify(button !== null)
            const image = findChild(button, data.before ? "wizardButtonLeadingIcon" : "wizardButtonFollowingIcon")
            verify(image !== null)
            tryCompare(image, "status", Image.Ready)
            verifyIconCentered(button)

            button.width = Style.wizardInlineButtonMinimumWidth
            verifyIconCentered(button)
            const icon = buttonIcon(button)
            compare(icon.height, Style.smallIconSize)
            if (data.before) {
                compare(icon.width, Style.smallIconSize)
            }
        }

        function test_wizardButtonLabelChanges_data() {
            return [{ tag: "leading", before: true }, { tag: "following", before: false }]
        }

        function test_wizardButtonLabelChanges(data) {
            const button = createTemporaryObject(wizardButtonComponent, testRoot, {
                width: Style.wizardInlineButtonMinimumWidth,
                iconBeforeText: data.before
            })
            verify(button !== null)
            const label = findChild(button, "wizardButtonText")
            verify(label !== null)
            compare(label.visible, false)
            verifyIconCentered(button)

            for (const properties of [{ text: "Action", suffix: "" }, { text: "", suffix: "Suffix" }]) {
                button.text = properties.text
                button.textSuffix = properties.suffix
                waitForPolish(button.contentItem)
                compare(label.visible, true)
                const icon = buttonIcon(button)
                compare(icon.width, Style.smallIconSize)
                const iconLeft = icon.mapToItem(button, 0, 0).x
                const labelLeft = label.mapToItem(button, 0, 0).x
                if (data.before) {
                    verify(iconLeft + icon.width <= labelLeft)
                } else {
                    verify(labelLeft + label.width <= iconLeft)
                }
            }

            button.text = ""
            button.textSuffix = ""
            compare(label.visible, false)
            verifyIconCentered(button)
        }

        function test_chatViewShowsThinkingState() {
            assistantTestSetup.controller.loadData()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.selectConversationAndCompleteMessages()
            const chatView = createChatView()
            const thinkingLabel = findChild(chatView, "assistantThinkingLabel")
            const retryButton = findChild(chatView, "assistantRetryResponseButton")

            verify(thinkingLabel !== null)
            verify(retryButton !== null)
            compare(thinkingLabel.visible, false)
            compare(retryButton.visible, true)
            mouseClick(retryButton)
            tryCompare(thinkingLabel, "visible", true)
            compare(thinkingLabel.text, "Assistant is thinking…")
        }

        function test_taskTypeDelegateSelectsTaskType() {
            createdObject = taskTypeDelegateComponent.createObject(testRoot)
            verify(createdObject !== null)
            compare(createdObject.objectName, "assistantTaskTypeDelegate")
            compare(createdObject.checked, false)

            mouseClick(createdObject)

            compare(assistantTestSetup.controller.selectedTaskTypeId,
                "core:text2text:summarize")
            compare(createdObject.checked, true)
        }

        function test_taskViewRetriesTask() {
            assistantTestSetup.seedTask()
            const taskView = createTaskView()
            const taskList = findChild(taskView, "assistantTaskList")

            verify(taskList !== null)
            tryCompare(taskList, "count", 1)
            tryVerify(() => taskList.width > 0 && taskList.height > 0)
            taskList.forceLayout()
            tryVerify(() => taskList.itemAtIndex(0) !== null)
            const delegate = taskList.itemAtIndex(0)
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
            tryVerify(() => taskList.width > 0 && taskList.height > 0)
            taskList.forceLayout()
            tryVerify(() => taskList.itemAtIndex(0) !== null)
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
