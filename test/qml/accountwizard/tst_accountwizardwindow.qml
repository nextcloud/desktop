/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtTest
import com.nextcloud.desktopclient

import "../../../src/gui/wizard/qml"

Item {
    id: testRoot

    width: 200
    height: 100

    Component {
        id: controllerComponent

        AccountWizardController {}
    }

    Component {
        id: windowComponent

        AccountWizardWindow {}
    }

    TestCase {
        property var wizard: null

        name: "AccountWizardWindow"
        when: windowShown

        function createWizard(step) {
            const controller = createTemporaryObject(controllerComponent, testRoot, { currentStep: step })
            verify(controller)
            wizard = windowComponent.createObject(testRoot, { controller: controller, visible: true })
            verify(wizard)
            return wizard
        }

        // Destroy the window before its temporary controller.
        function cleanup() {
            if (wizard) {
                wizard.destroy()
                wizard = null
                wait(0)
            }
            Theme.wizardHideSelfHostButton = false
            Theme.wizardHideSignupButton = false
        }

        function findVisibleButton(item, text) {
            if (item.visible && item.text === text && item.hasOwnProperty("clicked")) {
                return item
            }
            for (const child of item.children) {
                const found = findVisibleButton(child, text)
                if (found) {
                    return found
                }
            }
            return null
        }

        function test_buttonVisibility_data() {
            return [
                { tag: "default", hideSignup: false, hideSelfHost: false, step: AccountWizardController.ServerStep, signupVisible: true, selfHostVisible: true },
                { tag: "hide signup", hideSignup: true, hideSelfHost: false, step: AccountWizardController.ServerStep, signupVisible: false, selfHostVisible: true },
                { tag: "hide Self-host", hideSignup: false, hideSelfHost: true, step: AccountWizardController.ServerStep, signupVisible: true, selfHostVisible: false },
                { tag: "hide both", hideSignup: true, hideSelfHost: true, step: AccountWizardController.ServerStep, signupVisible: false, selfHostVisible: false },
                { tag: "other step default", hideSignup: false, hideSelfHost: false, step: AccountWizardController.BasicAuthStep, signupVisible: false, selfHostVisible: false },
                { tag: "other step hide signup", hideSignup: true, hideSelfHost: false, step: AccountWizardController.BasicAuthStep, signupVisible: false, selfHostVisible: false },
                { tag: "other step hide Self-host", hideSignup: false, hideSelfHost: true, step: AccountWizardController.BasicAuthStep, signupVisible: false, selfHostVisible: false },
                { tag: "other step hide both", hideSignup: true, hideSelfHost: true, step: AccountWizardController.BasicAuthStep, signupVisible: false, selfHostVisible: false }
            ]
        }

        function test_buttonVisibility(data) {
            Theme.wizardHideSignupButton = data.hideSignup
            Theme.wizardHideSelfHostButton = data.hideSelfHost
            const wizard = createWizard(data.step)
            compare(findVisibleButton(wizard.contentItem, "Sign up") !== null, data.signupVisible)
            compare(findVisibleButton(wizard.contentItem, "Self-host") !== null, data.selfHostVisible)
            if (data.step === AccountWizardController.ServerStep) {
                verify(findVisibleButton(wizard.contentItem, "Log in"))
            }
        }

        function test_signupClick() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            const button = findVisibleButton(wizard.contentItem, "Sign up")
            verify(button)
            compare(wizard.controller.signupRequests, 0)
            mouseClick(button)
            compare(wizard.controller.signupRequests, 1)
        }

        function test_signupDisabledWhileBusy() {
            const wizard = createWizard(AccountWizardController.ServerStep)
            const button = findVisibleButton(wizard.contentItem, "Sign up")
            verify(button)
            wizard.controller.busy = true
            tryCompare(button, "enabled", false)
            mouseClick(button)
            compare(wizard.controller.signupRequests, 0)
        }
    }
}
