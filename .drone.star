#
# We are building GCC with make and Clang with ninja, the combinations are more
# or less arbitrarily chosen. We just want to check that both compilers and both
# CMake generators work. It's unlikely that a specific generator only breaks
# with a specific compiler.
#

dir = {
    "base": "/drone",
}

def main(ctx):
    build_trigger = {
        "ref": [
            "refs/heads/master",
            "refs/heads/2.**",
            "refs/tags/**",
            "refs/pull/**",
        ],
    }
    cron_trigger = {
        "event": [
            "cron",
        ],
    }
    pipelines = [
        # check the format of gui test code
        gui_tests_format(build_trigger),
        # Check starlark
        check_starlark(
            ctx,
            build_trigger,
        ),
        # Build changelog
        changelog(
            ctx,
            trigger = build_trigger,
            depends_on = [],
        ),
        build_and_test_client(
            ctx,
            "clang",
            "clang++",
            "Debug",
            "Ninja",
            trigger = build_trigger,
        ),
        gui_tests(ctx, trigger = build_trigger, version = "latest"),
        notification(
            name = "build",
            trigger = build_trigger,
            depends_on = [
                "check-starlark",
                "changelog",
                "clang-debug-ninja",
            ],
        ),
    ]
    cron_pipelines = [
        # Build client
        build_and_test_client(
            ctx,
            "gcc",
            "g++",
            "Release",
            "Unix Makefiles",
            trigger = cron_trigger,
        ),
        build_and_test_client(
            ctx,
            "clang",
            "clang++",
            "Debug",
            "Ninja",
            trigger = cron_trigger,
        ),
        gui_tests(ctx, trigger = cron_trigger),
    ]

    if ctx.build.event == "cron":
        return cron_pipelines
    else:
        return pipelines

def whenOnline(dict):
    if not "when" in dict:
        dict["when"] = {}

    if not "instance" in dict:
        dict["when"]["instance"] = []

    dict["when"]["instance"].append("drone.owncloud.com")

    return dict

def whenPush(dict):
    if not "when" in dict:
        dict["when"] = {}

    if not "event" in dict:
        dict["when"]["event"] = []

    dict["when"]["event"].append("push")

    return dict

def from_secret(name):
    return {
        "from_secret": name,
    }

def check_starlark(ctx, trigger = {}, depends_on = []):
    return {
        "kind": "pipeline",
        "type": "docker",
        "name": "check-starlark",
        "steps": [
            {
                "name": "format-check-starlark",
                "image": "owncloudci/bazel-buildifier",
                "pull": "always",
                "commands": [
                    "buildifier --mode=check .drone.star",
                ],
            },
            {
                "name": "show-diff",
                "image": "owncloudci/bazel-buildifier",
                "pull": "always",
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
        "depends_on": depends_on,
        "trigger": trigger,
    }

def build_and_test_client(ctx, c_compiler, cxx_compiler, build_type, generator, trigger = {}, depends_on = []):
    build_command = "ninja" if generator == "Ninja" else "make"
    pipeline_name = c_compiler + "-" + build_type.lower() + "-" + build_command
    build_dir = "build-" + pipeline_name

    return {
        "kind": "pipeline",
        "name": pipeline_name,
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": [
                     {
                         "name": "submodules",
                         "image": "docker:git",
                         "commands": [
                             "git submodule update --init --recursive",
                         ],
                     },
                 ] +
                 build_client(ctx, c_compiler, cxx_compiler, build_type, generator, build_command, build_dir) +
                 [
                     {
                         "name": "ctest",
                         "image": "owncloudci/client:2.9-amd64",
                         "pull": "always",
                         "environment": {
                             "LC_ALL": "C.UTF-8",
                         },
                         "commands": [
                             'cd "' + build_dir + '"',
                             "useradd -m -s /bin/bash tester",
                             "chown -R tester:tester .",
                             "su-exec tester ctest --output-on-failure -LE nodrone",
                         ],
                     },
                 ],
        "trigger": trigger,
        "depends_on": depends_on,
    }

def gui_tests(ctx, trigger = {}, depends_on = [], filterTags = [], version = "daily-master-qa"):
    pipeline_name = "GUI-tests"
    build_dir = "build-" + pipeline_name
    squish_parameters = "--retry 1 --reportgen stdout --reportgen json,/drone/src/test/guiTestReport"

    if (len(filterTags) > 0):
        for tags in filterTags:
            squish_parameters += " --tags " + tags
            pipeline_name += "-" + tags

    return {
        "kind": "pipeline",
        "name": pipeline_name,
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": [
                     {
                         "name": "submodules",
                         "image": "docker:git",
                         "commands": [
                             "git submodule update --init --recursive",
                         ],
                     },
                 ] +
                 installCore(version) +
                 setupServerAndApp(2) +
                 fixPermissions() +
                 owncloudLog() +
                 setGuiTestReportDir() +
                 build_client(ctx, "gcc", "g++", "Debug", "Ninja", "ninja", build_dir) +
                 [
                     {
                         "name": "GUItests",
                         "image": "owncloudci/squish:qt512",
                         "pull": "always",
                         "environment": {
                             "LICENSEKEY": from_secret("squish_license_server"),
                             "CLIENT_REPO": "/drone/src/",
                             "MIDDLEWARE_URL": "http://testmiddleware:3000/",
                             "BACKEND_HOST": "http://owncloud/",
                             "SECURE_BACKEND_HOST": "https://owncloud/",
                             "SERVER_INI": "/drone/src/test/gui/drone/server.ini",
                             "SQUISH_PARAMETERS": squish_parameters,
                         },
                     },
                 ] +
                 showGuiTestResult(),
        "services": testMiddleware() +
                    owncloudService() +
                    databaseService(),
        "trigger": trigger,
        "depends_on": depends_on,
    }

def build_client(ctx, c_compiler, cxx_compiler, build_type, generator, build_command, build_dir):
    return [
        {
            "name": "cmake",
            "image": "owncloudci/client:2.9-amd64",
            "pull": "always",
            "environment": {
                "LC_ALL": "C.UTF-8",
            },
            "commands": [
                'mkdir -p "' + build_dir + '"',
                'cd "' + build_dir + '"',
                'cmake -G"' + generator + '" -DCMAKE_C_COMPILER="' + c_compiler + '" -DCMAKE_CXX_COMPILER="' + cxx_compiler + '" -DCMAKE_BUILD_TYPE="' + build_type + '" -DBUILD_TESTING=1 -DWITH_LIBCLOUDPROVIDERS=ON -S ..',
            ],
        },
        {
            "name": build_command,
            "image": "owncloudci/client:2.9-amd64",
            "pull": "always",
            "environment": {
                "LC_ALL": "C.UTF-8",
            },
            "commands": [
                'cd "' + build_dir + '"',
                build_command + " -j4",
            ],
        },
    ]

def gui_tests_format(trigger):
    return {
        "kind": "pipeline",
        "type": "docker",
        "name": "guitestformat",
        "steps": [
            {
                "name": "black",
                "image": "cytopia/black",
                "pull": "always",
                "commands": [
                    "cd /drone/src/test/gui",
                    "black --check --diff .",
                ],
            },
        ],
        "trigger": trigger,
    }

def changelog(ctx, trigger = {}, depends_on = []):
    repo_slug = ctx.build.source_repo if ctx.build.source_repo else ctx.repo.slug
    result = {
        "kind": "pipeline",
        "type": "docker",
        "name": "changelog",
        "clone": {
            "disable": True,
        },
        "steps": [
            {
                "name": "clone",
                "image": "plugins/git-action:1",
                "pull": "always",
                "settings": {
                    "actions": [
                        "clone",
                    ],
                    "remote": "https://github.com/%s" % (repo_slug),
                    "branch": ctx.build.source if ctx.build.event == "pull_request" else "master",
                    "path": "/drone/src",
                    "netrc_machine": "github.com",
                    "netrc_username": from_secret("github_username"),
                    "netrc_password": from_secret("github_token"),
                },
            },
            {
                "name": "generate",
                "image": "toolhippie/calens:latest",
                "pull": "always",
                "commands": [
                    "calens >| CHANGELOG.md",
                ],
            },
            {
                "name": "diff",
                "image": "owncloudci/alpine:latest",
                "pull": "always",
                "commands": [
                    "git diff",
                ],
            },
            {
                "name": "output",
                "image": "toolhippie/calens:latest",
                "pull": "always",
                "commands": [
                    "cat CHANGELOG.md",
                ],
            },
            {
                "name": "publish",
                "image": "plugins/git-action:1",
                "pull": "always",
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
                    "netrc_username": from_secret("github_username"),
                    "netrc_password": from_secret("github_token"),
                },
                "when": {
                    "ref": {
                        "exclude": [
                            "refs/pull/**",
                            "refs/tags/**",
                        ],
                    },
                },
            },
        ],
        "trigger": trigger,
        "depends_on": depends_on,
    }

    return result

def make(target, path, image = "owncloudci/transifex:latest"):
    return {
        "name": target,
        "image": image,
        "pull": "always",
        "environment": {
            "TX_TOKEN": from_secret("tx_token"),
        },
        "commands": [
            'cd "' + path + '"',
            "make " + target,
        ],
    }

def notification(name, depends_on = [], trigger = {}):
    trigger = dict(trigger)
    if not "status" in trigger:
        trigger["status"] = []

    trigger["status"].append("success")
    trigger["status"].append("failure")

    return {
        "kind": "pipeline",
        "name": "notifications-" + name,
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "clone": {
            "disable": True,
        },
        "steps": [
            {
                "name": "notification",
                "image": "plugins/slack",
                "pull": "always",
                "settings": {
                    "webhook": from_secret("slack_webhook"),
                    "channel": "desktop-ci",
                },
            },
        ],
        "trigger": trigger,
        "depends_on": depends_on,
    }

def databaseService():
    service = {
        "name": "mysql",
        "image": "mysql:8.0",
        "pull": "always",
        "environment": {
            "MYSQL_USER": "owncloud",
            "MYSQL_PASSWORD": "owncloud",
            "MYSQL_DATABASE": "owncloud",
            "MYSQL_ROOT_PASSWORD": "owncloud",
        },
        "command": ["--default-authentication-plugin=mysql_native_password"],
    }
    return [service]

def installCore(version):
    stepDefinition = {
        "name": "install-core",
        "image": "owncloudci/core",
        "pull": "always",
        "settings": {
            "version": version,
            "core_path": "/drone/src/server",
            "db_type": "mysql",
            "db_name": "owncloud",
            "db_host": "mysql",
            "db_username": "owncloud",
            "db_password": "owncloud",
        },
    }
    return [stepDefinition]

def setupServerAndApp(logLevel):
    return [{
        "name": "setup-owncloud-server",
        "image": "owncloudci/php:7.4",
        "pull": "always",
        "commands": [
            "cd /drone/src/server/",
            "php occ a:e testing",
            "php occ config:system:set trusted_domains 1 --value=owncloud",
            "php occ log:manage --level %s" % logLevel,
            "php occ config:list",
            "php occ config:system:set skeletondirectory --value=/var/www/owncloud/server/apps/testing/data/tinySkeleton",
            "php occ config:system:set sharing.federation.allowHttpFallback --value=true --type=bool",
        ],
    }]

def owncloudService():
    return [{
        "name": "owncloud",
        "image": "owncloudci/php:7.4",
        "pull": "always",
        "environment": {
            "APACHE_WEBROOT": "/drone/src/server/",
            "APACHE_CONFIG_TEMPLATE": "ssl",
            "APACHE_SSL_CERT_CN": "server",
            "APACHE_SSL_CERT": "%s/%s.crt" % (dir["base"], "server"),
            "APACHE_SSL_KEY": "%s/%s.key" % (dir["base"], "server"),
            "APACHE_LOGGING_PATH": "/dev/null",
        },
        "commands": [
            "cat /etc/apache2/templates/base >> /etc/apache2/templates/ssl",
            "/usr/local/bin/apachectl -e debug -D FOREGROUND",
        ],
    }]

def testMiddleware():
    return [{
        "name": "testmiddleware",
        "image": "owncloudci/nodejs:14",
        "pull": "always",
        "environment": {
            "MIDDLEWARE_HOST": "testmiddleware",
            "BACKEND_HOST": "http://owncloud",
        },
        "commands": [
            ". ./.drone.env",
            "git clone https://github.com/owncloud/owncloud-test-middleware.git /drone/src/middleware",
            "cd /drone/src/middleware",
            "git checkout $MIDDLEWARE_COMMITID",
            "yarn install",
            "yarn start",
        ],
    }]

def owncloudLog():
    return [{
        "name": "owncloud-log",
        "image": "owncloud/ubuntu:16.04",
        "pull": "always",
        "detach": True,
        "commands": [
            "tail -f /drone/src/server/data/owncloud.log",
        ],
    }]

def fixPermissions():
    return [{
        "name": "fix-permissions",
        "image": "owncloudci/php:7.4",
        "pull": "always",
        "commands": [
            "cd /drone/src/server",
            "chown www-data * -R",
        ],
    }]

def setGuiTestReportDir():
    return [{
        "name": "create-gui-test-report-directory",
        "image": "owncloud/ubuntu:16.04",
        "pull": "always",
        "commands": [
            "mkdir /drone/src/test/guiTestReport",
            "chmod ugo+rwx /drone/src/test/guiTestReport",
        ],
    }]

def showGuiTestResult():
    return [{
        "name": "show-gui-test-result",
        "image": "python",
        "pull": "always",
        "commands": [
            "python /drone/src/test/gui/TestLogParser.py /drone/src/test/guiTestReport/results.json",
        ],
        "when": {
            "status": [
                "failure",
            ],
        },
    }]
