#
# We are building GCC with make and Clang with ninja, the combinations are more
# or less arbitrarily chosen. We just want to check that both compilers and both
# CMake generators work. It's unlikely that a specific generator only breaks
# with a specific compiler.
#

def main(ctx):
    translations_trigger = {
        "cron": ["translations"],
    }
    pipelines = [
        # Build client and docs
        build_and_test_client(ctx, "gcc", "g++", "Release", "Unix Makefiles"),
        build_and_test_client(ctx, "clang", "clang++", "Debug", "Ninja"),
        build_client_docs(ctx),
        notification(
            name = "build",
            depends_on = [
                "gcc-release-make",
                "clang-debug-ninja",
                "build-docs",
            ],
        ),

        # Sync translations
        update_translations(
            ctx,
            "client",
            "translations",
            read_image = "rabits/qt:5.12-desktop",
            trigger = translations_trigger,
        ),
        update_translations(
            ctx,
            "nsis",
            "admin/win/nsi/l10n",
            write_image = "python:2.7-stretch",
            trigger = translations_trigger,
            depends_on = ["translations-client"],  # needs to run after translations-client because drone-git-push does not rebase before pushing
        ),
        notification(
            name = "translations",
            trigger = translations_trigger,
            depends_on = [
                "translations-client",
                "translations-nsis",
            ],
        ),
    ]

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

def build_and_test_client(ctx, c_compiler, cxx_compiler, build_type, generator):
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
            {
                "name": "cmake",
                "image": "owncloudci/client",
                "pull": "always",
                "environment": {
                    "LC_ALL": "C.UTF-8",
                },
                "commands": [
                    'mkdir -p "' + build_dir + '"',
                    'cd "' + build_dir + '"',
                    'cmake -G"' + generator + '" -DCMAKE_C_COMPILER="' + c_compiler + '" -DCMAKE_CXX_COMPILER="' + cxx_compiler + '" -DCMAKE_BUILD_TYPE="' + build_type + '" -DBUILD_TESTING=1 ..',
                ],
            },
            {
                "name": build_command,
                "image": "owncloudci/client",
                "pull": "always",
                "environment": {
                    "LC_ALL": "C.UTF-8",
                },
                "commands": [
                    'cd "' + build_dir + '"',
                    build_command + " -j4",
                ],
            },
            {
                "name": "ctest",
                "image": "owncloudci/client",
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
        "trigger": {
            "ref": [
                "refs/heads/master",
                "refs/tags/**",
                "refs/pull/**",
            ],
        },
    }

def build_client_docs(ctx):
    return {
        "kind": "pipeline",
        "name": "build-docs",
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": [
            whenOnline({
                "name": "cache-restore",
                "image": "plugins/s3-cache:1",
                "pull": "always",
                "settings": {
                    "endpoint": from_secret("cache_s3_endpoint"),
                    "access_key": from_secret("cache_s3_access_key"),
                    "secret_key": from_secret("cache_s3_secret_key"),
                    "restore": True,
                },
            }),
            {
                "name": "docs-deps",
                "image": "owncloudci/nodejs:11",
                "pull": "always",
                "commands": [
                    "cd docs/",
                    "yarn install",
                ],
            },
            {
                "name": "docs-validate",
                "image": "owncloudci/nodejs:11",
                "pull": "always",
                "commands": [
                    "cd docs/",
                    "yarn validate",
                ],
            },
            {
                "name": "docs-build",
                "image": "owncloudci/nodejs:11",
                "pull": "always",
                "commands": [
                    "cd docs/",
                    "yarn antora",
                ],
            },
            {
                "name": "docs-pdf",
                "image": "owncloudci/asciidoctor:latest",
                "pull": "always",
                "commands": [
                    "cd docs/",
                    "make pdf",
                ],
            },
            {
                "name": "docs-artifacts",
                "image": "owncloud/ubuntu:latest",
                "pull": "always",
                "commands": [
                    "tree docs/public/",
                ],
            },
            whenOnline(whenPush({
                "name": "cache-rebuild",
                "image": "plugins/s3-cache:1",
                "pull": "always",
                "settings": {
                    "endpoint": from_secret("cache_s3_endpoint"),
                    "access_key": from_secret("cache_s3_access_key"),
                    "secret_key": from_secret("cache_s3_secret_key"),
                    "rebuild": True,
                    "mount": "docs/cache",
                },
            })),
            whenOnline(whenPush({
                "name": "cache-flush",
                "image": "plugins/s3-cache:1",
                "pull": "always",
                "settings": {
                    "endpoint": from_secret("cache_s3_endpoint"),
                    "access_key": from_secret("cache_s3_access_key"),
                    "secret_key": from_secret("cache_s3_secret_key"),
                    "flush": True,
                    "flush_age": 14,
                },
            })),
            whenOnline(whenPush({
                "name": "upload-pdf",
                "image": "plugins/s3-sync:1",
                "pull": "always",
                "environment": {
                    "AWS_ACCESS_KEY_ID": from_secret("aws_access_key_id"),
                    "AWS_SECRET_ACCESS_KEY": from_secret("aws_secret_access_key"),
                },
                "settings": {
                    "bucket": "uploads",
                    "endpoint": "https://doc.owncloud.com",
                    "path_style": True,
                    "source": "docs/build/",
                    "target": "/deploy/",
                    "delete": False,
                },
            })),
        ],
        "trigger": {
            "ref": [
                "refs/heads/master",
                "refs/tags/**",
                "refs/pull/**",
            ],
        },
    }

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

def update_translations(ctx, name, path, read_image = "owncloudci/transifex:latest", write_image = "owncloudci/transifex:latest", trigger = {}, depends_on = []):
    return {
        "kind": "pipeline",
        "name": "translations-" + name,
        "platform": {
            "os": "linux",
            "arch": "amd64",
        },
        "steps": [
            make("l10n-read", path, read_image),
            make("l10n-push", path),
            make("l10n-pull", path),
            make("l10n-write", path, write_image),
            make("l10n-clean", path),
            # keep time window for commit races as small as possible
            {
                "name": "update-repo-before-commit",
                "image": "docker:git",
                "commands": [
                    "git stash",
                    "git pull --ff-only origin +refs/heads/$${DRONE_BRANCH}",
                    '[ "$(git stash list)" = "" ] || git stash pop',
                ],
            },
            whenOnline({
                "name": "commit",
                "image": "appleboy/drone-git-push",
                "pull": "always",
                "settings": {
                    "ssh_key": from_secret("git_push_ssh_key"),
                    "author_name": "ownClouders",
                    "author_email": "devops@owncloud.com",
                    "remote_name": "origin",
                    "branch": "${DRONE_BRANCH}",
                    "empty_commit": False,
                    "commit": True,
                    "commit_message": "[tx] updated " + name + " translations from transifex",
                    "no_verify": True,
                },
            }),
        ],
        "trigger": trigger,
        "depends_on": depends_on,
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
