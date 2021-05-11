import names
import squish
from helpers.SetupClientHelper import getClientDetails


class AccountConnectionWizard():
    SERVER_ADDRESS_BOX = names.leUrl_OCC_PostfixLineEdit
    NEXT_BUTTON = names.owncloudWizard_qt_passive_wizardbutton1_QPushButton
    USERNAME_BOX = names.leUsername_QLineEdit
    PASSWORD_BOX = names.lePassword_QLineEdit
    SELECT_LOCAL_FOLDER = names.pbSelectLocalFolder_QPushButton
    DIRECTORY_NAME_BOX = names.fileNameEdit_QLineEdit
    CHOOSE_BUTTON = names.qFileDialog_Choose_QPushButton
    CONNECT_BUTTON = names.owncloudWizard_qt_passive_wizardbutton1_QPushButton


    def __init__(self):
        pass

    def addAccount(self, context):  
        server, user, password, localfolder =  getClientDetails(context)
        
        squish.mouseClick(squish.waitForObject(self.SERVER_ADDRESS_BOX))
        squish.type(squish.waitForObject(self.SERVER_ADDRESS_BOX), server)
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))
        squish.mouseClick(squish.waitForObject(self.SERVER_ADDRESS_BOX))
        squish.type(squish.waitForObject(self.USERNAME_BOX), user)
        squish.type(squish.waitForObject(self.USERNAME_BOX), "<Tab>")
        squish.type(squish.waitForObject(self.PASSWORD_BOX), password)
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))
        squish.clickButton(squish.waitForObject(self.SELECT_LOCAL_FOLDER))
        squish.mouseClick(squish.waitForObject(self.DIRECTORY_NAME_BOX))
        squish.type(squish.waitForObject(self.DIRECTORY_NAME_BOX), localfolder)
        squish.clickButton(squish.waitForObject(self.CHOOSE_BUTTON))
        squish.clickButton(squish.waitForObject(self.CONNECT_BUTTON))
