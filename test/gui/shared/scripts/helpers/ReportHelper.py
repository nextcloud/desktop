import os
import glob
import shutil
import test
import squish
import squishinfo

from helpers.ConfigHelper import get_config
from helpers.FilesHelper import prefix_path_namespace


def get_screenrecords_path():
    return os.path.join(get_config("guiTestReportDir"), "screenrecords")


def get_screenshots_path():
    return os.path.join(get_config("guiTestReportDir"), "screenshots")


def is_video_enabled():
    return (
        get_config("record_video_on_failure")
        or get_config("retrying")
        and not reached_video_limit()
    )


def reached_video_limit():
    video_report_dir = get_screenrecords_path()
    if not os.path.exists(video_report_dir):
        return False
    entries = [f for f in os.scandir(video_report_dir) if f.is_file()]
    return len(entries) >= get_config("video_record_limit")


def save_video_recording(filename, test_failed):
    try:
        # do not throw if stopVideoCapture() fails
        test.stopVideoCapture()
    except:
        test.log("Failed to stop screen recording")

    if not (video_dir := squishinfo.resultDir):
        video_dir = squishinfo.testCase
    else:
        test_case = "/".join(squishinfo.testCase.split("/")[-2:])
        video_dir = os.path.join(video_dir, test_case)
    video_dir = os.path.join(video_dir, "attachments")

    # if the test failed
    # move videos to the screenrecords directory
    if test_failed:
        video_files = glob.glob(f"{video_dir}/**/*.mp4", recursive=True)
        screenrecords_dir = get_screenrecords_path()
        if not os.path.exists(screenrecords_dir):
            os.makedirs(screenrecords_dir)
        # reverse the list to get the latest video first
        video_files.reverse()
        for idx, video in enumerate(video_files):
            if idx:
                file_parts = filename.rsplit(".", 1)
                filename = f"{file_parts[0]}_{idx+1}.{file_parts[1]}"
            shutil.move(video, os.path.join(screenrecords_dir, filename))
    # remove the video directory
    shutil.rmtree(prefix_path_namespace(video_dir))


def take_screenshot(filename):
    directory = get_screenshots_path()
    if not os.path.exists(directory):
        os.makedirs(directory)
    try:
        squish.saveDesktopScreenshot(os.path.join(directory, filename))
    except:
        test.log("Failed to save screenshot")
