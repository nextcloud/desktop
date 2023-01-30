import os
import builtins
from tempfile import gettempdir
from configparser import ConfigParser

# map environment variables to config keys
CONFIG_ENV_MAP = {
    'localBackendUrl': 'BACKEND_HOST',
    'secureLocalBackendUrl': 'SECURE_BACKEND_HOST',
    'maxSyncTimeout': 'MAX_SYNC_TIMEOUT',
    'minSyncTimeout': 'MIN_SYNC_TIMEOUT',
    'lowestSyncTimeout': 'LOWEST_SYNC_TIMEOUT',
    'middlewareUrl': 'MIDDLEWARE_URL',
    'clientLogFile': 'CLIENT_LOG_FILE',
    'clientRootSyncPath': 'CLIENT_ROOT_SYNC_PATH',
    'tempFolderPath': 'TEMP_FOLDER_PATH',
    'clientConfigDir': 'CLIENT_CONFIG_DIR',
    'guiTestReportDir': 'GUI_TEST_REPORT_DIR',
    'ocis': 'OCIS',
}

# default config values
CONFIG = {
    'localBackendUrl': 'https://localhost:9200/',
    'secureLocalBackendUrl': 'https://localhost:9200/',
    'maxSyncTimeout': 10,
    'minSyncTimeout': 5,
    'lowestSyncTimeout': 1,
    'middlewareUrl': 'http://localhost:3000/',
    'clientLogFile': '-',
    'clientRootSyncPath': '/tmp/client-bdd/',
    'tempFolderPath': gettempdir(),
    'clientConfigDir': '/tmp/owncloud-client/',
    'guiTestReportDir': os.path.abspath('../reports/'),
    'ocis': False,
}

READONLY_CONFIG = list(CONFIG_ENV_MAP.keys())


def init_config():
    global CONFIG, CONFIG_ENV_MAP
    # try reading configs from config.ini
    cfg = ConfigParser()
    try:
        script_path = os.path.dirname(os.path.realpath(__file__))
        config_path = os.path.abspath(
            os.path.join(script_path, '..', '..', '..', 'config.ini')
        )
        if cfg.read(config_path):
            for key, _ in CONFIG.items():
                if key in CONFIG_ENV_MAP:
                    value = cfg.get('DEFAULT', CONFIG_ENV_MAP[key])
                    if value:
                        if key == 'ocis':
                            CONFIG[key] = value == 'true'
                        else:
                            CONFIG[key] = value
    except Exception:
        pass

    # read and override configs from environment variables
    for key, value in CONFIG_ENV_MAP.items():
        if os.environ.get(value):
            if key == 'ocis':
                CONFIG[key] = os.environ.get(value) == 'true'
            else:
                CONFIG[key] = os.environ.get(value)

    # Set the default values if empty
    for key, value in CONFIG.items():
        if key == 'maxSyncTimeout' or key == 'minSyncTimeout':
            CONFIG[key] = builtins.int(value)
        elif (
            key == 'clientRootSyncPath'
            or key == 'tempFolderPath'
            or key == 'clientConfigDir'
            or key == 'guiTestReportDir'
            or key == 'localBackendUrl'
            or key == 'middlewareUrl'
        ):
            # make sure there is always one trailing slash
            CONFIG[key] = value.rstrip('/') + '/'


def get_config(key=None):
    global CONFIG
    if key:
        return CONFIG[key]
    return CONFIG


def set_config(key, value):
    global CONFIG, READONLY_CONFIG
    if key in READONLY_CONFIG:
        raise Exception('Cannot set read-only config: %s' % key)
    CONFIG[key] = value
    return CONFIG


def clear_scenario_config():
    global CONFIG, READONLY_CONFIG
    initial_config = {}
    for key in READONLY_CONFIG:
        initial_config[key] = CONFIG[key]

    CONFIG = initial_config
