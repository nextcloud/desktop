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
            conversationId: 42
            canDelete: true
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
            width: 700
            assistantController: assistantTestSetup.controller
            attachments: []
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

    Component {
        id: messageTextComponent
        Assistant.AssistantMessageText {
            assistantController: assistantTestSetup.controller
            width: 300
            height: implicitHeight
            textColor: Style.wizardPrimaryText
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

        function test_tabFromInputReachesConversationPicker() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.selectConversationAndCompleteMessages()
            const input = findChild(window, "assistantQuestionInput")
            const picker = findChild(window, "assistantConversationPicker")
            verify(input !== null)
            verify(picker !== null)
            verify(picker.enabled)
            window.requestActivate()
            input.forceActiveFocus(Qt.TabFocusReason)
            tryCompare(input, "activeFocus", true)
            compare(input.KeyNavigation.tab, picker)
            assistantTestSetup.keyClickInWindow(window, Qt.Key_Tab, Qt.NoModifier)
            tryCompare(picker, "activeFocus", true)
            assistantTestSetup.keyClickInWindow(window, Qt.Key_Tab, Qt.ShiftModifier)
            tryCompare(input, "activeFocus", true)
        }

        function test_longTitlesStayOnOneLine() {
            createdObject = conversationDelegateComponent.createObject(testRoot, {
                title: "Long title\n".repeat(100)
            })
            const label = findChild(createdObject, "assistantConversationTitle")
            verify(label !== null)
            verify(label.text.length > 1)
            verify(label.text.startsWith("Long title"))
            verify(label.text.length <= Style.assistantConversationTitleMaximumLength)
            verify(label.text.endsWith("…"))
            verify(label.text.indexOf("\n") === -1)
            compare(label.maximumLineCount, 1)
            createdObject.title = "Short title"
            compare(label.text, "Short title")
        }

        function test_titleLengthBoundaryKeepsVisibleText() {
            const maximum = Style.assistantConversationTitleMaximumLength
            createdObject = conversationDelegateComponent.createObject(testRoot, {
                title: "A".repeat(maximum)
            })
            const label = findChild(createdObject, "assistantConversationTitle")
            compare(label.text, "A".repeat(maximum))
            createdObject.title += "B"
            compare(label.text, "A".repeat(maximum - 1) + "…")
            createdObject.title = "Short title"
            compare(label.text, "Short title")
        }

        function test_actionButtons_data() {
            return [{ tag: "confirm", confirm: true }, { tag: "cancel", confirm: false }]
        }

        function test_actionButtons(data) {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.seedPendingActions()
            const panel = findChild(window, "assistantActionConfirmation")
            verify(panel !== null)
            const scroll = findChild(panel, "assistantActionDetailsScroll")
            tryVerify(() => scroll.contentHeight > scroll.height)
            const confirm = findChild(panel, "assistantConfirmActionsButton")
            const cancel = findChild(panel, "assistantCancelActionsButton")
            verify(confirm.enabled)
            verify(cancel.enabled)
            mouseClick(data.confirm ? confirm : cancel)
            verify(!confirm.enabled)
            verify(!cancel.enabled)
            assistantTestSetup.completeActionReply()
            compare(assistantTestSetup.pendingDecision(), data.confirm ? 1 : 0)
            compare(assistantTestSetup.controller.pendingActions.length, 0)
        }

        function test_invalidActionCannotBeConfirmed() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.seedPendingActions(true)
            const panel = findChild(window, "assistantActionConfirmation")
            verify(panel !== null)
            verify(!findChild(panel, "assistantConfirmActionsButton").enabled)
            verify(findChild(panel, "assistantCancelActionsButton").enabled)
        }

        function test_downloadLinkShowsPointingHand() {
            createdObject = messageTextComponent.createObject(testRoot, {
                text: "[Download](https://cloud.example/file) plain text"
            })
            const textItem = findChild(createdObject, "assistantMessageText")
            const hover = findChild(createdObject, "assistantMessageLinkHover")
            verify(hover !== null)
            const linkPosition = textItem.positionToRectangle(2)
            mouseMove(textItem, linkPosition.x + linkPosition.width / 2, linkPosition.y + linkPosition.height / 2)
            tryCompare(textItem, "hoveredLink", "https://cloud.example/file")
            tryCompare(hover, "hovered", true)
            compare(hover.cursorShape, Qt.PointingHandCursor)
            const plainPosition = textItem.positionToRectangle(textItem.length - 1)
            mouseMove(textItem, plainPosition.x + plainPosition.width / 2, plainPosition.y + plainPosition.height / 2)
            tryCompare(textItem, "hoveredLink", "")
            compare(hover.cursorShape, Qt.IBeamCursor)
        }

        function test_messageLinksUseBubbleColors_data() {
            return [
                { tag: "own-markdown", role: "user", markdown: true },
                { tag: "own-url", role: "user", markdown: false },
                { tag: "assistant-markdown", role: "assistant", markdown: true },
                { tag: "assistant-url", role: "assistant", markdown: false }
            ]
        }

        function test_messageLinksUseBubbleColors(data) {
            failOnWarning(/ReferenceError:|TypeError:|Binding loop/)
            const url = "https://example.test/ticket#comment"
            const label = data.markdown ? "Ticket" : url
            createdObject = messageDelegateComponent.createObject(testRoot, {
                messageRole: data.role,
                messageText: (data.markdown ? "[" + label + "](" + url + ")" : url) + " plain text"
            })
            verify(createdObject !== null)
            const textItem = findChild(createdObject, "assistantMessageText")
            const expectedColor = data.role === "user" ? Style.wizardSelectedText : Style.wizardRadioAccent
            textItem.select(0, label.length)
            tryCompare(textItem.cursorSelection, "color", expectedColor)
            verify(textItem.cursorSelection.font.underline)
            compare(textItem.selectedText, label)
            textItem.deselect()
            const linkPosition = textItem.positionToRectangle(2)
            compare(textItem.linkAt(linkPosition.x, linkPosition.y + linkPosition.height / 2), url)
            compare(textItem.getText(0, textItem.length), label + " plain text")

            createdObject.messageRole = data.role === "user" ? "assistant" : "user"
            textItem.select(0, label.length)
            tryCompare(textItem.cursorSelection, "color", data.role === "user" ? Style.wizardRadioAccent : Style.wizardSelectedText)
            textItem.deselect()
            createdObject.messageText = "No link here"
            tryCompare(textItem, "length", "No link here".length)
            compare(textItem.getText(0, textItem.length), "No link here")
        }

        function test_longConversationScrollsWithoutMovingTheComposer() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.seedLongConversation()
            const picker = findChild(window, "assistantConversationPicker")
            const title = findChild(picker, "assistantConversationTitle")
            verify(title.text.length <= Style.assistantConversationTitleMaximumLength)
            const list = findChild(window, "assistantMessageList")
            list.forceLayout()
            const viewport = findChild(list, "assistantMessageViewport")
            const scrollbar = findChild(list, "assistantConversationScrollBar")
            const input = findChild(window, "assistantQuestionInput")
            tryVerify(() => viewport.contentHeight > viewport.height)
            tryVerify(() => scrollbar.visible)
            verify(viewport.height > 0)
            tryCompare(scrollbar, "height", list.availableHeight)
            const scrollbarPosition = scrollbar.mapToItem(list, 0, 0)
            compare(scrollbarPosition.x + scrollbar.width, list.width)
            compare(scrollbarPosition.y, list.topPadding)
            const inputPosition = input.mapToItem(window.contentItem, 0, 0)
            viewport.positionViewAtBeginning()
            const start = viewport.contentY
            scrollbar.increase()
            tryVerify(() => viewport.contentY > start)
            compare(input.mapToItem(window.contentItem, 0, 0).y, inputPosition.y)
            verify(inputPosition.y + input.height <= window.height)
        }

        function test_verticalWheelOverMessageScrollsConversation() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.seedLongConversation()
            const list = findChild(window, "assistantMessageList")
            const viewport = findChild(list, "assistantMessageViewport")
            list.forceLayout()
            tryVerify(() => viewport.contentHeight > viewport.height)
            verify(waitForRendering(viewport))
            list.forceLayout()
            viewport.positionViewAtBeginning()
            verify(waitForRendering(viewport))
            tryVerify(() => list.itemAtIndex(0) !== null)
            const message = findChild(list.itemAtIndex(0), "assistantMessageText")
            const point = message.mapFromItem(viewport,
                message.mapToItem(viewport, message.width / 2, 0).x, viewport.height / 2)
            verify(point.x > 0 && point.x < message.width)
            verify(point.y > 0 && point.y < message.height)
            const start = viewport.contentY
            mouseWheel(message, point.x, point.y, 0, -120)
            tryVerify(() => viewport.contentY > start)
            viewport.cancelFlick()
            const scrolled = viewport.contentY
            const backPoint = message.mapFromItem(viewport,
                message.mapToItem(viewport, message.width / 2, 0).x, viewport.height / 2)
            verify(backPoint.x > 0 && backPoint.x < message.width)
            verify(backPoint.y > 0 && backPoint.y < message.height)
            mouseWheel(message, backPoint.x, backPoint.y, 0, 120)
            tryVerify(() => viewport.contentY < scrolled)
        }

        function test_wideCodeHasHorizontalScrolling() {
            createdObject = messageTextComponent.createObject(testRoot, {
                text: "```yaml\ncommand: " + "very_long_argument_".repeat(50) + "\n```"
            })
            const viewport = findChild(createdObject, "assistantMessageTextViewport")
            const scrollbar = findChild(createdObject, "assistantMessageHorizontalScrollBar")
            tryVerify(() => viewport.contentWidth > viewport.width)
            tryVerify(() => scrollbar.visible)
            compare(createdObject.width, 300)
            const textItem = findChild(createdObject, "assistantMessageText")
            mouseWheel(textItem, viewport.width / 2, viewport.height / 2, -120, 0)
            tryVerify(() => viewport.contentX > 0)
            viewport.cancelFlick()
            createdObject.text = "Short response"
            tryVerify(() => !scrollbar.visible)
            tryCompare(viewport, "contentX", 0)
            tryCompare(createdObject, "height", textItem.implicitHeight)
        }

        function test_linkedNextcloudFilesUseGenericAttachments() {
            createdObject = messageDelegateComponent.createObject(testRoot, {
                messageText: "[Picture](/index.php/f/12) [Duplicate](https://cloud.example/f/12) "
                    + "[Archive](/f/13) [External](https://other.example/f/14) "
                    + "[Different port](https://cloud.example:8443/f/15) "
                    + "`[Code](/f/16)` [Not a file](/apps/analytics/r/17)",
                attachments: [{ fileId: 12, taskId: -1, type: "File" }]
            })
            verify(createdObject !== null)
            compare(createdObject.displayedAttachments.length, 2)
            compare(createdObject.displayedAttachments[1].fileId, 13)
            createdObject.messageText = "No file link"
            compare(createdObject.displayedAttachments.length, 1)
        }

        function test_taskOutputLinksCreatePreviewCards_data() {
            return [
                { tag: "assistant", path: "/ocs/v2.php/apps/assistant/api/v1/task/1221224/output-file/14/download" },
                { tag: "task-processing", path: "/ocs/v2.php/taskprocessing/tasks/1221224/file/14" }
            ]
        }

        function test_taskOutputLinksCreatePreviewCards(data) {
            createdObject = messageDelegateComponent.createObject(testRoot, {
                messageRole: "assistant",
                messageText: "Here is your image: [Download the image](https://cloud.example" + data.path + ")",
                attachments: []
            })
            verify(createdObject !== null)
            compare(createdObject.displayedAttachments.length, 1)
            compare(createdObject.displayedAttachments[0].fileId, 14)
            compare(createdObject.displayedAttachments[0].taskId, 1221224)
            const card = findChild(createdObject, "assistantAttachmentDelegate")
            verify(card !== null)
            tryVerify(() => card.attachment !== null && !card.attachment.loading)
            const preview = findChild(card, "assistantAttachmentPreview")
            tryCompare(preview, "status", Image.Ready)
            verify(findChild(card, "assistantAttachmentDownload").enabled)
            compare(card.attachment.downloadUrl.toString(),
                "https://cloud.example/ocs/v2.php/apps/assistant/api/v1/task/1221224/output-file/14/download")
        }

        function test_taskOutputLinksMergeAndRejectUnrelatedUrls() {
            createdObject = messageDelegateComponent.createObject(testRoot, {
                messageText: "[Output](/ocs/v2.php/taskprocessing/tasks/42/file/14) "
                    + "[Duplicate](/ocs/v2.php/apps/assistant/api/v1/task/42/output-file/14/download) "
                    + "[External](https://other.example/ocs/v2.php/taskprocessing/tasks/42/file/15) "
                    + "[Invalid](/ocs/v2.php/taskprocessing/tasks/0/file/16) "
                    + "[Share action](/ocs/v2.php/apps/assistant/api/v1/task/42/output-file/17/share) "
                    + "`[Code](/ocs/v2.php/taskprocessing/tasks/42/file/18)`",
                attachments: [{ fileId: 14, taskId: -1, type: "File" }]
            })
            verify(createdObject !== null)
            compare(createdObject.displayedAttachments.length, 1)
            compare(createdObject.displayedAttachments[0].fileId, 14)
            compare(createdObject.displayedAttachments[0].taskId, 42)
        }

        function test_attachmentOnlyMessagesShowPreviewsAndGenericFiles() {
            const window = createAssistantWindow()
            assistantTestSetup.completeChatLoad()
            assistantTestSetup.seedAttachments()
            const list = findChild(window, "assistantMessageList")
            tryCompare(list, "count", 2)
            list.forceLayout()
            tryVerify(() => list.itemAtIndex(0) !== null && list.itemAtIndex(1) !== null)
            const imageMessage = list.itemAtIndex(0)
            const imageCard = findChild(imageMessage, "assistantAttachmentDelegate")
            verify(imageCard !== null)
            tryVerify(() => imageCard.attachment !== null && !imageCard.attachment.loading)
            compare(findChild(imageMessage, "assistantMessageText").visible, false)
            compare(findChild(imageCard, "assistantAttachmentName").text, "Picture.png")
            const preview = findChild(imageCard, "assistantAttachmentPreview")
            tryCompare(preview, "status", Image.Ready)
            verify(preview.visible)
            tryVerify(() => imageMessage.height > preview.height)

            const fileCard = findChild(list.itemAtIndex(1), "assistantAttachmentDelegate")
            verify(fileCard !== null)
            tryVerify(() => fileCard.attachment !== null && !fileCard.attachment.loading)
            compare(findChild(fileCard, "assistantAttachmentName").text, "Archive.zip")
            verify(findChild(fileCard, "assistantAttachmentPreviewError").visible)
            verify(findChild(fileCard, "assistantAttachmentDownload").enabled)
            verify(findChild(fileCard, "assistantAttachmentOpen").visible)
            const retry = findChild(fileCard, "assistantAttachmentRetry")
            mouseClick(retry)
            tryVerify(() => !fileCard.attachment.loading)
            verify(fileCard.attachment.previewFailed)

            assistantTestSetup.controller.startNewChat()
            tryCompare(list, "count", 0)
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

        function test_conversationTrashAppearsOnHoverAndFocus() {
            createdObject = conversationDelegateComponent.createObject(testRoot, {
                pickerHighlightedIndex: -1
            })
            verify(createdObject !== null)
            const button = findChild(createdObject, "assistantDeleteConversationButton")
            verify(button !== null)
            mouseMove(testRoot, testRoot.width - 1, testRoot.height - 1)
            tryCompare(button, "opacity", 0)
            mouseMove(createdObject, 10, createdObject.height / 2)
            tryCompare(button, "opacity", 1)
            mouseMove(testRoot, testRoot.width - 1, testRoot.height - 1)
            tryCompare(button, "opacity", 0)
            button.forceActiveFocus(Qt.TabFocusReason)
            tryCompare(button, "opacity", 1)
            verify(button.activeFocus)
            createdObject.canDelete = false
            verify(!button.enabled)
        }

        function test_conversationDeletionConfirmation_data() {
            return [
                { tag: "cancel", action: "cancel", selected: true },
                { tag: "escape", action: "escape", selected: true },
                { tag: "delete-active", action: "confirm", selected: true },
                { tag: "keyboard", action: "keyboard", selected: true },
                { tag: "delete-unopened", action: "confirm", selected: false },
                { tag: "failure", action: "fail", selected: true }
            ]
        }

        function test_conversationDeletionConfirmation(data) {
            failOnWarning(/ReferenceError:/)
            assistantTestSetup.controller.loadData()
            assistantTestSetup.completeChatLoad()
            if (data.selected) {
                assistantTestSetup.selectConversationAndCompleteMessages()
            }
            const chatView = createChatView()
            const picker = findChild(chatView, "assistantConversationPicker")
            mouseClick(picker)
            tryCompare(picker.popup, "visible", true)
            const list = findChild(picker, "assistantConversationDropdownList")
            verify(list !== null)
            list.forceLayout()
            tryVerify(() => list.itemAtIndex(0) !== null)
            const row = list.itemAtIndex(0)
            mouseMove(row, 10, row.height / 2)
            const trash = findChild(row, "assistantDeleteConversationButton")
            tryCompare(trash, "opacity", 1)
            if (data.action === "keyboard") {
                picker.forceActiveFocus()
                keyClick(Qt.Key_Delete)
            } else {
                mouseClick(trash)
            }
            tryCompare(picker.popup, "visible", false)
            compare(list.model, null)
            tryCompare(list, "count", 0)
            const selectedId = data.selected ? 42 : -1
            compare(assistantTestSetup.controller.selectedChatConversationId, selectedId)
            compare(assistantTestSetup.deleteConversationCount, 0)
            const dialog = findChild(chatView, "assistantDeleteConversationDialog")
            verify(dialog !== null)
            tryCompare(dialog, "opened", true)
            compare(dialog.conversationId, 42)
            compare(dialog.conversationTitle, "Conversation")
            if (data.action === "escape") {
                keyClick(Qt.Key_Escape)
            } else {
                mouseClick(findChild(dialog, data.action === "cancel"
                    ? "assistantDeleteConversationCancelButton" : "assistantDeleteConversationConfirmButton"))
            }
            tryCompare(chatView, "conversationToDelete", -1)
            compare(chatView.conversationTitleToDelete, "")
            if (data.action === "cancel" || data.action === "escape") {
                tryCompare(picker, "activeFocus", true)
                compare(assistantTestSetup.deleteConversationCount, 0)
                compare(picker.count, 1)
                compare(assistantTestSetup.controller.selectedChatConversationId, selectedId)
                return
            }
            compare(assistantTestSetup.deleteConversationCount, 1)
            compare(assistantTestSetup.lastDeletedConversationId, 42)
            compare(picker.count, 1)
            verify(assistantTestSetup.controller.requestInProgress)
            assistantTestSetup.completeConversationDeletion(data.action !== "fail")
            compare(picker.count, data.action === "fail" ? 1 : 0)
            compare(assistantTestSetup.controller.selectedChatConversationId, data.action === "fail" ? selectedId : -1)
            verify(!assistantTestSetup.controller.requestInProgress)
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
            const picker = findChild(chatView, "assistantConversationPicker")
            compare(assistantTestSetup.controller.requestInProgress, true)
            compare(picker.enabled, true)
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
