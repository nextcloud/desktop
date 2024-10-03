import os
import subprocess
import squish


def get_clipboard_text():
    try:
        return squish.getClipboardText()
    except:
        # Retry after 2 seconds
        squish.snooze(2)
        return squish.getClipboardText()


def authorize_via_webui(username, password, login_type='oidc'):
    script_path = os.path.dirname(os.path.realpath(__file__))

    webui_path = os.path.join(script_path, '..', '..', '..', 'webUI')
    os.chdir(webui_path)

    envs = {
        'OC_USERNAME': username.strip('"'),
        'OC_PASSWORD': password.strip('"'),
        'OC_AUTH_URL': get_clipboard_text(),
    }
    proc = subprocess.run(
        f'pnpm run {login_type}-login',
        capture_output=True,
        shell=True,
        env={**os.environ, **envs},
        check=False,
    )
    if proc.returncode:
        if proc.stderr.decode('utf-8'):
            raise OSError(proc.stderr.decode('utf-8'))
        raise OSError(proc.stdout.decode('utf-8'))
    os.chdir(script_path)
