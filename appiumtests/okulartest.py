#!/usr/bin/env python3

# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2024 Antoine Herlicq <antoine.herlicq@enioka.com>
# SPDX-FileContributor: 2024 Antoine Gonzalez <antoine.gonzalez@enioka.com>

"""
IMPORTANT:

Because of some responsive behaviors, the application NEEDS to be launched in a big enough compositor, otherwise some
buttons may be hidden which will break the tests. Because the app does not take up the whole compositor, you probably
want at least 1920x1080. Set the following environment variables before calling the selenium command:
COMPOSITOR_WIDTH=1920 COMPOSITOR_HEIGHT=1080
"""

import os
import tempfile
import time
import unittest
import shutil

from appium import webdriver
from appium.options.common.base import AppiumOptions
from appium.webdriver.common.appiumby import AppiumBy
from selenium.webdriver import ActionChains, Keys
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC


class OkularTests(unittest.TestCase):
    def find_element_by_class_name(self, type, name):
        return self.driver.find_element(by=AppiumBy.CLASS_NAME, value=f"[{type} | {name}]")

    def find_element_by_name(self, element):
        return self.driver.find_element(by=AppiumBy.NAME, value=element)

    def find_element_by_accessibility_id(self, element):
        return self.driver.find_element(by=AppiumBy.ACCESSIBILITY_ID, value=element)

    def assert_current_page_has_text(self, text):
        time.sleep(0.2)  # Let the page number update
        current_page_number = self.find_element_by_accessibility_id("PageLabelEdit").text
        ActionChains(self.driver).key_down(Keys.CONTROL).send_keys("f").key_up(Keys.CONTROL).perform()
        ActionChains(self.driver).send_keys(text).send_keys(Keys.ENTER).perform()
        searched_text_page_number = self.find_element_by_accessibility_id("PageLabelEdit").text
        self.find_element_by_accessibility_id("FindBar.QToolButton").click()
        self.assertEqual(current_page_number, searched_text_page_number)
        ActionChains(self.driver).send_keys(Keys.ESCAPE).perform()  # Close the search tool
        ActionChains(self.driver).send_keys(Keys.ESCAPE).perform()  # Remove the search highlight

    @classmethod
    def setUpClass(self):
        options = AppiumOptions()
        options.set_capability("app", "org.kde.okular.desktop")

        max_attempts = 3
        attempts = 0
        while attempts < max_attempts:
            try:
                self.driver = webdriver.Remote(command_executor='http://127.0.0.1:4723', options=options)
                break
            except Exception as e:
                attempts += 1
                time.sleep(1)
        self.driver.implicitly_wait(10)
        self.wait = WebDriverWait(self.driver, 10)

    @classmethod
    def tearDownClass(self):
        max_attempts = 3
        attempts = 0
        while attempts < max_attempts:
            try:
                self.driver.quit()
                break
            except Exception as e:
                attempts += 1
                time.sleep(1)

    def test_a_app_installed(self):
        self.assertTrue(self.driver.is_app_installed("okular"))

    def test_b_app_runs(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = os.path.abspath(temp_dir)
            current_directory = os.path.dirname(os.path.abspath(__file__))
            shutil.copy(f"{current_directory}/20yearsofKDE.pdf", temp_path)

            print("\n########## 01 - Open the file and check the page number\n")
            self.find_element_by_accessibility_id("openButton").click()
            self.find_element_by_class_name("text", "").click()
            ActionChains(self.driver).send_keys(f"{temp_path}/20yearsofKDE.pdf").perform()
            self.find_element_by_class_name("push button", "Open").click()
            time.sleep(5)  # Give time for the file to load on a slow FS
            main_bar_buttons = self.driver.find_elements(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.okular::Shell#1.mainToolBar.miniBar.HoverButton")
            page_selector_button = main_bar_buttons[0]
            self.assertEquals(page_selector_button.get_attribute("name"), "186")

            print("\n########## 02 - Press Ctrl+G (Go-To shortcut) and go to 'real' page 38\n")
            main = self.find_element_by_accessibility_id("QApplication.okular::Shell#1")
            ActionChains(self.driver).move_to_element(main).key_down(Keys.CONTROL).send_keys("g").key_up(Keys.CONTROL).perform()
            go_to_page = self.find_element_by_class_name("spin button", "Page:")
            ActionChains(self.driver).move_to_element(go_to_page).send_keys("38").perform()
            self.find_element_by_class_name("push button", "OK").click()
            self.assert_current_page_has_text("Torben broke the cloning machine")

            print("\n########## 03 - Click View > Presentation, move 5 pages further, then 5 pages back, exit presentation mode and check that we are back on the original page\n")
            self.find_element_by_class_name("menu item", "View").click()
            self.find_element_by_class_name("menu item", "Presentation").click()
            ActionChains(self.driver).send_keys(Keys.ENTER).perform()
            ActionChains(self.driver).send_keys(Keys.DOWN, Keys.DOWN, Keys.DOWN, Keys.DOWN, Keys.DOWN).perform()
            ActionChains(self.driver).send_keys(Keys.UP, Keys.UP, Keys.UP, Keys.UP, Keys.UP).perform()
            ActionChains(self.driver).send_keys(Keys.ESCAPE).perform()
            self.assert_current_page_has_text("Torben broke the cloning machine")

            # NOTE: Sometimes the View menu does not close properly after clicking an action if we re-open it too fast.
            #  Added the escape keys and wait time just in case.
            print("\n########## 04 - Rotate the page left twice, then right twice\n")
            for _ in range(2):
                self.find_element_by_class_name("menu item", "View").click()
                self.find_element_by_class_name("menu item", "Orientation").click()
                self.find_element_by_class_name("menu item", "Rotate Left").click()
                ActionChains(self.driver).send_keys(Keys.ESCAPE + Keys.ESCAPE).perform()  # Make sure the menu is closed
                time.sleep(0.2)  # Make sure the menu is closed
            for _ in range(2):
                self.find_element_by_class_name("menu item", "View").click()
                self.find_element_by_class_name("menu item", "Orientation").click()
                self.find_element_by_class_name("menu item", "Rotate Right").click()
                ActionChains(self.driver).send_keys(Keys.ESCAPE + Keys.ESCAPE).perform()  # Make sure the menu is closed
                time.sleep(0.2)  # Make sure the menu is closed

            print("\n########## 05 - Navigate 5 pages forward then 5 pages back using the page navigation buttons\n")
            main_bar_buttons = self.driver.find_elements(by=AppiumBy.ACCESSIBILITY_ID, value="QApplication.okular::Shell#1.mainToolBar.miniBar.HoverButton")
            previous_page_button = main_bar_buttons[0]
            next_page_button = main_bar_buttons[2]
            for _ in range(5):
                next_page_button.click()
                time.sleep(0.1)
            self.assert_current_page_has_text("Re-writing the bylaws proved to be a challenge")
            for _ in range(5):
                previous_page_button.click()
                time.sleep(0.1)
            self.assert_current_page_has_text("Torben broke the cloning machine")

            print("\n########## 06 - Try various zoom interactions\n")
            self.find_element_by_class_name("menu item", "View").click()
            self.find_element_by_class_name("menu item", "Zoom to 100%").click()
            zoom_in = self.find_element_by_class_name("push button", "Zoom In")
            for _ in range(5):
                zoom_in.click()
            self.find_element_by_class_name("menu item", "View").click()
            self.find_element_by_class_name("menu item", "Fit Width").click()

            print("\n########## 07 - Click Settings > Configure Okular > Accessibility > Change colors > Apply > Ok\n")
            self.find_element_by_class_name("menu item", "Settings").click()
            self.find_element_by_accessibility_id("options_configure").click()
            self.find_element_by_name("Accessibility").click()
            self.find_element_by_class_name("check box", "Change colors").click()
            self.find_element_by_class_name("push button", "Apply").click()
            self.find_element_by_class_name("push button", "OK").click()

            print("\n########## 08 - Click Go > Go to Page and go to 'real' page 42\n")
            self.find_element_by_accessibility_id("QMenuBar.go").click()
            self.find_element_by_accessibility_id("go.go.go_goto_page").click()
            go_to_page = self.find_element_by_class_name("spin button", "Page:")
            ActionChains(self.driver).move_to_element(go_to_page).send_keys("42").perform()
            self.find_element_by_class_name("push button", "OK").click()
            self.assert_current_page_has_text("In 1999 I was elected to the KDE")

            # FIXME: Testing annotations is too complicated.
            #  - The click-and-hold requires too much accuracy and risks altering other elements which will break other tests
            #  - Proper testing of an annotation's existence appears unreliable and returns false positives too often
            #  Would be nice to have down the line but doesn't work as-is
            print("\n########## 09 - SKIPPED\n")
            """
            print("\n########## 09 - Press F6 (Annotations shortcut) and ALT+1 (Highlight shortcut), highlight text, add an annotation, then exit highlight mode with ALT+1 again\n")
            ActionChains(self.driver).send_keys(Keys.F6).perform()
            ActionChains(self.driver).key_down(Keys.ALT).send_keys("1").key_up(Keys.ALT).perform()
            ActionChains(self.driver).move_to_element(self.find_element_by_accessibility_id("QMenuBar.file")).move_by_offset(180, 304).click_and_hold().move_by_offset(872, 364).release().perform()
            ActionChains(self.driver).move_to_element(self.find_element_by_accessibility_id("QMenuBar.file")).move_by_offset(400, 500).double_click().perform()
            ActionChains(self.driver).send_keys("Very interesting text! I should read more about this topic.").perform()
            ActionChains(self.driver).key_down(Keys.ALT).send_keys("1").key_up(Keys.ALT).perform()
            self.assert_annotation_has_text("Very interesting text! I should read more about this topic.")
            # Check that the annotation exists --- FIXME: needs a rework, too many false positives
            ActionChains(self.driver).key_down(Keys.CONTROL).send_keys("f").key_up(Keys.CONTROL).perform()
            ActionChains(self.driver).send_keys(text).send_keys(Keys.ENTER).perform()
            self.driver.implicitly_wait(0)
            #self.assertTrue(len(self.driver.find_elements(by=AppiumBy.ACCESSIBILITY_ID, value="information")) == 0)
            with self.assertRaises(NoSuchElementException):
                self.driver.find_elements(by=AppiumBy.ACCESSIBILITY_ID, value="information")
            self.driver.implicitly_wait(10)
            """

            print("\n########## 10 - Click File > Save then File > Quit\n")
            self.find_element_by_class_name("menu item", "File").click()
            self.find_element_by_accessibility_id("file_save").click()
            #self.find_element_by_class_name("menu item", "File").click() ////the menu is not closed by the previous action so we don't need to open it again
            self.find_element_by_accessibility_id("file_quit").click()

if __name__ == '__main__':
    unittest.main()
