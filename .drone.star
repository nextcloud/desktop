#
# We are building GCC with make and Clang with ninja, the combinations are more
# or less arbitrarily chosen. We just want to check that both compilers and both
# CMake generators work. It's unlikely that a specific generator only breaks
# with a specific compiler.
#

DEFAULT_PHP_VERSION = "7.4"

MYSQL = "mysql:8.0"
OC_CI_ALPINE = "owncloudci/alpine:latest"
OC_CI_BAZEL_BUILDIFIER = "owncloudci/bazel-buildifier"
OC_CI_CLIENT = "owncloudci/client:latest"
OC_CI_CORE = "owncloudci/core"
OC_CI_DRONE_SKIP_PIPELINE = "owncloudci/drone-skip-pipeline"
OC_CI_NODEJS = "owncloudci/nodejs:18"
OC_CI_PHP = "owncloudci/php:%s"
OC_CI_WAIT_FOR = "owncloudci/wait-for:latest"
OC_OCIS = "owncloud/ocis:%s"
OC_TEST_MIDDLEWARE = "owncloud/owncloud-test-middleware:1.8.7"
OC_UBUNTU = "owncloud/ubuntu:20.04"

# Eventually, we have to use image built on ubuntu
# Todo: update or remove the following images
# https://github.com/owncloud/client/issues/10070
OC_CI_CLIENT_FEDORA = "owncloudci/client:fedora-39-amd64"
OC_CI_SQUISH = "owncloudci/squish:fedora-39-7.2.1-qt66x-linux64"

PLUGINS_GIT_ACTION = "plugins/git-action:1"
PLUGINS_S3 = "plugins/s3"
PLUGINS_SLACK = "plugins/slack"
TOOLHIPPIE_CALENS = "toolhippie/calens:latest"

# npm packages to install
NPM_GHERLINT = "@gherlint/gherlint@1.1.0"

# secrets used in the pipeline
secrets = {
    "SQUISH_LICENSEKEY": "squish_license_server",
    "GITHUB_USERNAME": "github_username",
    "GITHUB_TOKEN": "github_token",  # not available for PRs
    "CACHE_ENDPOINT": "cache_public_s3_server",
    "CACHE_BUCKET": "cache_public_s3_bucket",
    "AWS_ACCESS_KEY_ID": "cache_public_s3_access_key",
    "AWS_SECRET_ACCESS_KEY": "cache_public_s3_secret_key",
    "ROCKETCHAT_WEBHOOK": "rocketchat_talk_webhook",
}

dir = {
    "base": "/drone/src",
    "server": "/drone/src/server",
    "guiTest": "/drone/src/test/gui",
    "guiTestReport": "/drone/src/test/gui/guiReportUpload",
    "build": "/drone/src/build",
    "pythonModules": "/usr/local/lib/python3.10/site-packages",
    "pythonModules64": "/usr/local/lib64/python3.10/site-packages",
}

notify_channels = {
    "desktop-ci": {
        "type": "channel",
    },
}

branch_ref = [
    "refs/heads/master",
    "refs/heads/5**",
]

trigger_ref = branch_ref + [
    "refs/tags/**",
    "refs/pull/**",
]

build_config = {
    "c_compiler": "clang",
    "cxx_compiler": "clang++",
    "build_type": "Debug",
    "generator": "Ninja",
    "command": "ninja",
}

pip_pipeline_volume = [
    {
        "name": "python",
        "temp": {},
    },
    {
        "name": "python64",
        "temp": {},
    },
]
pip_step_volume = [
    {
        "name": "python",
        "path": dir["pythonModules"],
    },
    {
        "name": "python64",
        "path": dir["pythonModules64"],
    },
]

config = {
    "gui-tests": {
        "servers": {
            "oc10": {
                "version": "latest",
                # comma separated list of tags to be used for filtering. E.g. "@tag1,@tag2"
                "tags": "~@skipOnOC10",
                "extra_apps": {
                    "oauth2": {
                        "enabled": False,
                        "command": "make dist",
                    },
                    "activity": {
                        "enabled": True,
                        "command": "make dist",
                    },
                },
                "skip_in_pr": True,
                "skip": False,
            },
            "ocis": {
                "version": "5.0",
                # comma separated list of tags to be used for filtering. E.g. "@tag1,@tag2"
                "tags": "~@skipOnOCIS",
                "skip": False,
            },
        },
    },
}

def main(ctx):
    pipelines = check_starlark() + \
                lint_gui_test() + \
                changelog(ctx)
    unit_tests = unit_test_pipeline(ctx)
    gui_tests = gui_test_pipeline(ctx)

    return pipelines + \
           unit_tests + \
           gui_tests + \
           pipelinesDependsOn(notification(), unit_tests + gui_tests)

def from_secret(name):
    return {
        "from_secret": secrets[name],
    }

def check_starlark():
    return [{
        "kind": "pipeline",
        "type": "docker",
        "name": "check-starlark",
        "steps": [
            {
                "name": "format-check-starlark",
                "image": OC_CI_BAZEL_BUILDIFIER,
                "commands": [
                    "buildifier --mode=check .drone.star",
                ],
            },
            {
                "name": "show-diff",
                "image": OC_CI_BAZEL_BUILDIFIER,
                "commands": [
                    "buildifier --mode=fix .drone.star",
                    "git diff",
                ],
                "when": {
                    "status": [
                        "failure",
                    ],
                },
            },
        ],
        "trigger": {
            "event": [
                "pull_request",
            ],
        },
    }]

def unit_test_pipeline(ctx):
    return [{
        "kind": "pipeline",
        "name": "unit-tests",
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": skipIfUnchanged(ctx, "unit-tests") +
                 build_client(OC_CI_CLIENT_FEDORA) +
                 unit_tests(OC_CI_CLIENT_FEDORA),
        "trigger": {
            "ref": trigger_ref,
        },
    }]

def gui_test_pipeline(ctx):
    pipelines = []
    for server, params in config["gui-tests"]["servers"].items():
        squish_parameters = [
            "--testsuite %s" % dir["guiTest"],
            "--reportgen html,%s" % dir["guiTestReport"],
            "--envvar QT_LOGGING_RULES=sync.httplogger=true;gui.socketapi=false",
            "--tags ~@skip",
            "--tags ~@skipOnLinux",
        ]

        if params.get("skip", False):
            continue
        if ctx.build.event == "pull_request" and params.get("skip_in_pr", False) and not "full-ci" in ctx.build.title.lower():
            continue

        # also skip in commit push
        if params.get("skip_in_pr", False) and ctx.build.event == "push":
            continue

        pipeline_name = "GUI-tests-%s" % server

        if params["tags"]:
            squish_parameters.append("--tags %s" % params["tags"])
        squish_parameters = " ".join(squish_parameters)

        steps = skipIfUnchanged(ctx, "gui-tests") + \
                build_client(OC_CI_SQUISH, False)

        services = testMiddlewareService(server)

        if server == "oc10":
            steps += installCore(params["version"]) + \
                     setupServerAndApp() + \
                     installExtraApps(params["extra_apps"]) + \
                     fixPermissions() + \
                     owncloudLog()
            services += owncloudService() + \
                        databaseService()
        else:
            steps += ocisService(params["version"]) + \
                     waitForService("ocis", "ocis:9200")

        steps += installPnpm() + \
                 install_python_modules() + \
                 setGuiTestReportDir() + \
                 gui_tests(squish_parameters, server) + \
                 uploadGuiTestLogs(ctx, server) + \
                 logGuiReports(ctx, server)

        pipelines.append({
            "kind": "pipeline",
            "name": pipeline_name,
            "platform": {
                "os": "linux",
                "arch": "amd64",
            },
            "steps": steps,
            "services": services,
            "trigger": {
                "ref": trigger_ref,
            },
            "volumes": [
                {
                    "name": "uploads",
                    "temp": {},
                },
            ] + pip_pipeline_volume,
        })
    return pipelines

def build_client(image = OC_CI_CLIENT, ctest = True):
    cmake_options = '-G"%s" -DCMAKE_BUILD_TYPE="%s"' % (build_config["generator"], build_config["build_type"])

    if image != OC_CI_SQUISH:
        cmake_options += ' -DCMAKE_C_COMPILER="%s" -DCMAKE_CXX_COMPILER="%s"' % (build_config["c_compiler"], build_config["cxx_compiler"])

    if ctest:
        cmake_options += " -DBUILD_TESTING=ON"
    else:
        cmake_options += " -DBUILD_TESTING=OFF"

    return [
        {
            "name": "build-client",
            "image": image,
            "environment": {
                "LC_ALL": "C.UTF-8",
            },
            "user": "0:0",
            "commands": [
                "mkdir -p %s" % dir["build"],
                "cd %s" % dir["build"],
                # generate build files
                "cmake %s -S .." % cmake_options,
                # build
                build_config["command"],
            ],
        },
    ]

def unit_tests(image = OC_CI_CLIENT):
    return [{
        "name": "ctest",
        "image": image,
        "environment": {
            "LC_ALL": "C.UTF-8",
        },
        "commands": [
            "cd %s" % dir["build"],
            "useradd -m -s /bin/bash tester",
            "chown -R tester:tester .",
            "su-exec tester ctest --output-on-failure -LE nodrone",
        ],
    }]

def gui_tests(squish_parameters = "", server_type = "oc10"):
    return [{
        "name": "GUItests",
        "image": OC_CI_SQUISH,
        "environment": {
            "LICENSEKEY": from_secret("SQUISH_LICENSEKEY"),
            "GUI_TEST_REPORT_DIR": dir["guiTestReport"],
            "CLIENT_REPO": dir["base"],
            "MIDDLEWARE_URL": "http://testmiddleware:3000/",
            "BACKEND_HOST": "http://owncloud/" if server_type == "oc10" else "https://ocis:9200",
            "SECURE_BACKEND_HOST": "https://owncloud/" if server_type == "oc10" else "https://ocis:9200",
            "OCIS": "true" if server_type == "ocis" else "false",
            "SERVER_INI": "%s/drone/server.ini" % dir["guiTest"],
            "SQUISH_PARAMETERS": squish_parameters,
            "STACKTRACE_FILE": "%s/stacktrace.log" % dir["guiTestReport"],
            "PLAYWRIGHT_BROWSERS_PATH": "%s/.playwright" % dir["base"],
            "OWNCLOUD_CORE_DUMP": 1,
            "SCREEN_RECORD_ON_FAILURE": False,
            # allow to use any available pnpm version
            "COREPACK_ENABLE_STRICT": 0,
        },
        "volumes": pip_step_volume,
    }]

def lint_gui_test():
    return [{
        "kind": "pipeline",
        "type": "docker",
        "name": "lint-gui-test",
        "steps": python_lint() + gherkin_lint(),
        "trigger": {
            "event": [
                "pull_request",
            ],
        },
    }]

def python_lint():
    return [{
        "name": "python-lint",
        "image": OC_CI_ALPINE,
        "commands": [
            "apk add --no-cache py3-pylint black",
            "make -C %s python-lint" % dir["guiTest"],
        ],
    }]

def gherkin_lint():
    return [{
        "name": "gherkin-lint",
        "image": OC_CI_NODEJS,
        "commands": [
            "npm install -g %s" % NPM_GHERLINT,
            "make -C %s gherkin-lint" % dir["guiTest"],
        ],
    }]

def changelog(ctx):
    return [{
        "kind": "pipeline",
        "type": "docker",
        "name": "changelog",
        "steps": [
            {
                "name": "generate",
                "image": TOOLHIPPIE_CALENS,
                "commands": [
                    "calens >| CHANGELOG.md",
                ],
            },
            {
                "name": "diff",
                "image": OC_CI_ALPINE,
                "commands": [
                    "git diff",
                ],
            },
            {
                "name": "output",
                "image": TOOLHIPPIE_CALENS,
                "commands": [
                    "cat CHANGELOG.md",
                ],
            },
            {
                "name": "publish",
                "image": PLUGINS_GIT_ACTION,
                "settings": {
                    "actions": [
                        "commit",
                        "push",
                    ],
                    "message": "Automated changelog update [skip ci]",
                    "branch": "master",
                    "author_email": "devops@owncloud.com",
                    "author_name": "ownClouders",
                    "netrc_machine": "github.com",
                    "netrc_username": from_secret("GITHUB_USERNAME"),
                    "netrc_password": from_secret("GITHUB_TOKEN"),
                },
                "when": {
                    "ref": {
                        "exclude": [
                            "refs/pull/**",
                            "refs/tags/**",
                        ],
                    },
                    "branch": [
                        "master",
                    ],
                },
            },
        ],
        "trigger": {
            "ref": branch_ref + [
                "refs/pull/**",
            ],
            "event": {
                "exclude": [
                    "cron",
                ],
            },
        },
    }]

def notification():
    steps = [{
        "name": "create-template",
        "image": OC_CI_ALPINE,
        "environment": {
            "CACHE_ENDPOINT": from_secret("CACHE_ENDPOINT"),
            "CACHE_BUCKET": from_secret("CACHE_BUCKET"),
        },
        "commands": [
            "bash %s/drone/notification_template.sh %s" % (dir["guiTest"], dir["base"]),
        ],
    }]

    for channel, params in notify_channels.items():
        settings = {
            "webhook": from_secret("ROCKETCHAT_WEBHOOK"),
            "template": "file:%s/template.md" % dir["base"],
        }
        if params["type"] == "user":
            settings["recipient"] = channel
        else:
            settings["channel"] = channel

        steps.append(
            {
                "name": "notification-%s" % channel,
                "image": PLUGINS_SLACK,
                "settings": settings,
            },
        )

    return [{
        "kind": "pipeline",
        "name": "notifications",
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": steps,
        "trigger": {
            "event": [
                "cron",
                "tag",
            ],
            "status": [
                "success",
                "failure",
            ],
        },
    }]

def databaseService():
    return [{
        "name": "mysql",
        "image": MYSQL,
        "environment": {
            "MYSQL_USER": "owncloud",
            "MYSQL_PASSWORD": "owncloud",
            "MYSQL_DATABASE": "owncloud",
            "MYSQL_ROOT_PASSWORD": "owncloud",
        },
        "command": ["--default-authentication-plugin=mysql_native_password"],
    }]

def installCore(server_version = "latest"):
    return [{
        "name": "install-core",
        "image": OC_CI_CORE,
        "settings": {
            "version": server_version,
            "core_path": dir["server"],
            "db_type": "mysql",
            "db_name": "owncloud",
            "db_host": "mysql",
            "db_username": "owncloud",
            "db_password": "owncloud",
        },
    }]

def setupServerAndApp(logLevel = 2):
    return [{
        "name": "setup-owncloud-server",
        "image": OC_CI_PHP % DEFAULT_PHP_VERSION,
        "commands": [
            "cd %s" % dir["server"],
            "php occ a:e testing",
            "php occ config:system:set trusted_domains 1 --value=owncloud",
            "php occ log:manage --level %s" % logLevel,
            "php occ config:list",
            "php occ config:system:set skeletondirectory --value=/var/www/owncloud/server/apps/testing/data/tinySkeleton",
            "php occ config:system:set sharing.federation.allowHttpFallback --value=true --type=bool",
        ],
    }]

def installExtraApps(extra_apps = {}):
    commands = []
    for app, param in extra_apps.items():
        commands.append("ls %s/apps/%s || git clone --depth 1 https://github.com/owncloud/%s.git %s/apps/%s" % (dir["server"], app, app, dir["server"], app))
        if (param["command"] != ""):
            commands.append("cd %s/apps/%s" % (dir["server"], app))
            commands.append(param["command"])
        if param["enabled"]:
            commands.append("cd %s" % dir["server"])
            commands.append("php occ a:e %s" % app)

    return [{
        "name": "install-extra-apps",
        "image": OC_CI_PHP % DEFAULT_PHP_VERSION,
        "commands": commands,
    }]

def owncloudService():
    return [{
        "name": "owncloud",
        "image": OC_CI_PHP % DEFAULT_PHP_VERSION,
        "environment": {
            "APACHE_WEBROOT": dir["server"],
            "APACHE_CONFIG_TEMPLATE": "ssl",
            "APACHE_SSL_CERT_CN": "server",
            "APACHE_SSL_CERT": "%s/server.crt" % dir["base"],
            "APACHE_SSL_KEY": "%s/server.key" % dir["base"],
            "APACHE_LOGGING_PATH": "/dev/null",
        },
        "commands": [
            "cat /etc/apache2/templates/base >> /etc/apache2/templates/ssl",
            "/usr/local/bin/apachectl -e debug -D FOREGROUND",
        ],
    }]

def testMiddlewareService(server_type = "oc10"):
    environment = {
        "NODE_TLS_REJECT_UNAUTHORIZED": "0",
        "MIDDLEWARE_HOST": "testmiddleware",
        "REMOTE_UPLOAD_DIR": "/uploads",
    }

    if server_type == "ocis":
        environment["BACKEND_HOST"] = "https://ocis:9200"
        environment["TEST_WITH_GRAPH_API"] = "true"
        environment["RUN_ON_OCIS"] = "true"
    else:
        environment["BACKEND_HOST"] = "http://owncloud"

    return [{
        "name": "testmiddleware",
        "image": OC_TEST_MIDDLEWARE,
        "environment": environment,
        "volumes": [{
            "name": "uploads",
            "path": "/uploads",
        }],
    }]

def owncloudLog():
    return [{
        "name": "owncloud-log",
        "image": OC_UBUNTU,
        "detach": True,
        "commands": [
            "tail -f %s/data/owncloud.log" % dir["server"],
        ],
    }]

def fixPermissions():
    return [{
        "name": "fix-permissions",
        "image": OC_CI_PHP % DEFAULT_PHP_VERSION,
        "commands": [
            "cd %s" % dir["server"],
            "chown www-data * -R",
        ],
    }]

def ocisService(server_version = "latest"):
    return [{
        "name": "ocis",
        "image": OC_OCIS % server_version,
        "detach": True,
        "environment": {
            "OCIS_URL": "https://ocis:9200",
            "IDM_ADMIN_PASSWORD": "admin",
            "STORAGE_HOME_DRIVER": "ocis",
            "STORAGE_USERS_DRIVER": "ocis",
            "OCIS_INSECURE": True,
            "PROXY_ENABLE_BASIC_AUTH": True,
            "OCIS_LOG_LEVEL": "error",
            "OCIS_LOG_PRETTY": True,
            "OCIS_LOG_COLOR": True,
        },
        "commands": [
            "/usr/bin/ocis init",
            "/usr/bin/ocis server",
        ],
    }]

def waitForService(name, servers):
    if type(servers) == "string":
        servers = [servers]
    return [{
        "name": "wait-for-%s" % name,
        "image": OC_CI_WAIT_FOR,
        "commands": [
            "wait-for -it %s -t 300" % ",".join(servers),
        ],
    }]

def installPnpm():
    return [{
        "name": "pnpm-install",
        "image": OC_CI_NODEJS,
        "environment": {
            "PLAYWRIGHT_BROWSERS_PATH": "%s/.playwright" % dir["base"],
        },
        "commands": [
            'npm i -s -g -f "$(jq -r ".packageManager" < %s/webUI/package.json)"' % dir["guiTest"],
            "pnpm config set store-dir %s/.pnpm-store" % dir["base"],
            "make -C %s pnpm-install" % dir["guiTest"],
            # install required browser
            "make -C %s pnpm-install-chromium" % dir["guiTest"],
        ],
    }]

def install_python_modules():
    return [{
        "name": "install-python-modules",
        "image": OC_CI_SQUISH,
        "user": "0:0",
        "commands": [
            "make -C %s pip-install" % dir["guiTest"],
        ],
        "volumes": pip_step_volume,
    }]

def setGuiTestReportDir():
    return [{
        "name": "create-gui-test-report-directory",
        "image": OC_UBUNTU,
        "commands": [
            "mkdir %s/screenshots -p" % dir["guiTestReport"],
            "chmod 777 %s -R" % dir["guiTest"],
        ],
    }]

def uploadGuiTestLogs(ctx, server_type = "oc10"):
    trigger = {
        "status": [
            "failure",
        ],
        "event": [
            "cron",
            "tag",
            "pull_request",
        ],
    }
    if ctx.build.event == "tag":
        trigger["status"].append("success")

    return [{
        "name": "upload-gui-test-result",
        "image": PLUGINS_S3,
        "settings": {
            "bucket": from_secret("CACHE_BUCKET"),
            "endpoint": from_secret("CACHE_ENDPOINT"),
            "path_style": True,
            "source": "%s/**/*" % dir["guiTestReport"],
            "strip_prefix": "%s" % dir["guiTestReport"],
            "target": "/${DRONE_REPO}/${DRONE_BUILD_NUMBER}/%s/guiReportUpload" % server_type,
        },
        "environment": {
            "AWS_ACCESS_KEY_ID": from_secret("AWS_ACCESS_KEY_ID"),
            "AWS_SECRET_ACCESS_KEY": from_secret("AWS_SECRET_ACCESS_KEY"),
        },
        "when": trigger,
    }]

def logGuiReports(ctx, server_type):
    trigger = {
        "status": [
            "failure",
        ],
        "event": [
            "cron",
            "tag",
            "pull_request",
        ],
    }
    if ctx.build.event == "tag":
        trigger["status"].append("success")

    return [{
        "name": "log-GUI-reports",
        "image": OC_UBUNTU,
        "commands": [
            "bash %s/drone/log_reports.sh %s ${DRONE_REPO} ${DRONE_BUILD_NUMBER} %s" % (dir["guiTest"], dir["guiTestReport"], server_type),
        ],
        "when": trigger,
    }]

def skipIfUnchanged(ctx, type):
    if ("full-ci" in ctx.build.title.lower()):
        return []

    base = [
        "^.github/.*",
        "^.vscode/.*",
        "^changelog/.*",
        "README.md",
        ".gitignore",
        "CHANGELOG.md",
        "CONTRIBUTING.md",
        "COPYING",
        "COPYING.documentation",
    ]

    skip = []
    if type == "unit-tests":
        skip = base + [
            "^test/gui/.*",
        ]

    if type == "gui-tests":
        skip = base + [
            "^test/([^g]|g[^u]|gu[^i]).*",
        ]

    return [{
        "name": "skip-if-unchanged",
        "image": OC_CI_DRONE_SKIP_PIPELINE,
        "settings": {
            "ALLOW_SKIP_CHANGED": skip,
        },
        "when": {
            "event": [
                "pull_request",
            ],
        },
    }]

def stepDependsOn(steps = []):
    if type(steps) == dict:
        steps = [steps]
    return getPipelineNames(steps)

def getPipelineNames(pipelines = []):
    names = []
    for pipeline in pipelines:
        names.append(pipeline["name"])
    return names

def pipelineDependsOn(pipeline, dependant_pipelines):
    if "depends_on" in pipeline.keys():
        pipeline["depends_on"] = pipeline["depends_on"] + getPipelineNames(dependant_pipelines)
    else:
        pipeline["depends_on"] = getPipelineNames(dependant_pipelines)
    return pipeline

def pipelinesDependsOn(pipelines, dependant_pipelines):
    pipes = []
    for pipeline in pipelines:
        pipes.append(pipelineDependsOn(pipeline, dependant_pipelines))

    return pipes
