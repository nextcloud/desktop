import os
import subprocess
import squish


def getClipboardText():
    try:
        return squish.getClipboardText()
    except:
        # Retry after 2 seconds
        squish.snooze(2)
        return squish.getClipboardText()


def authorize_via_webui(username, password, login_type="oidc"):
    script_path = os.path.dirname(os.path.realpath(__file__))

    webui_path = os.path.join(script_path, "..", "..", "..", 'webUI')
    os.chdir(webui_path)

    envs = {
        'OC_USERNAME': username.strip('"'),
        'OC_PASSWORD': password.strip('"'),
        'OC_AUTH_URL': getClipboardText(),
    }
    proc = subprocess.run(
        "pnpm run %s-login" % login_type,
        capture_output=True,
        shell=True,
        env={**os.environ, **envs},
    )
    if proc.returncode != 0:
        if proc.stderr.decode('utf-8'):
            raise Exception(proc.stderr.decode('utf-8'))
        else:
            raise Exception(proc.stdout.decode('utf-8'))
    os.chdir(script_path)
