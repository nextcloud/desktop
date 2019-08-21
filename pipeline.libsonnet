#
# We are building GCC with make and Clang with ninja, the combinations are more
# or less arbitrarily chosen. We just want to check that both compilers and both
# CMake generators work. It's unlikely that a specific generator only breaks
# with a specific compiler.
#

local drone = {
  from_secret(secret)::
    {
      from_secret: secret
    },

  whenOnline:
    {
      when: (if 'when' in super then super.when else {}) {
        instance: [
          'drone.owncloud.com',
        ],
      },
    },

  whenPush:
    {
      when: (if 'when' in super then super.when else {}) {
        event: [
          'push',
        ],
      }
    },
};

{
  build_and_test_client(c_compiler, cxx_compiler, build_type, generator)::
    local build_command = if generator == "Ninja" then 'ninja' else 'make';
    local pipeline_name = c_compiler + '-' + std.asciiLower(build_type) + '-' + build_command;
    local build_dir = 'build-' + pipeline_name;

    {
      kind: 'pipeline',
      name: pipeline_name,
      platform: {
        os: 'linux',
        arch: 'amd64',
      },

      steps: [
        {
          name: 'cmake',
          image: 'owncloudci/client',
          pull: true,
          environment: {
            LC_ALL: 'C.UTF-8'
          },
          commands: [
            'mkdir -p "'+build_dir+'"',
            'cd "'+build_dir+'"',
            'cmake -G"'+generator+'" -DCMAKE_C_COMPILER="' + c_compiler + '" -DCMAKE_CXX_COMPILER="' + cxx_compiler + '" -DCMAKE_BUILD_TYPE="' + build_type + '" -DBUILD_TESTING=1 ..',
          ],
        },
        {
          name: build_command,
          image: 'owncloudci/client',
          pull: true,
          environment: {
            LC_ALL: 'C.UTF-8'
          },
          commands: [
            'cd "'+build_dir+'"',
            build_command + ' -j4',
          ],
        },
        {
          name: 'ctest',
          image: 'owncloudci/client',
          pull: true,
          environment: {
            LC_ALL: 'C.UTF-8'
          },
          commands: [
            'cd "'+build_dir+'"',
            'useradd -m -s /bin/bash tester',
            'chown -R tester:tester .',
            'su-exec tester ctest --output-on-failure',
          ],
        },
      ],
      trigger: {
        event: [
          'push',
          'pull_request',
          'tag',
        ],
      },
    },

  build_client_docs()::
    {
      kind: 'pipeline',
      name: 'build-docs',
      platform: {
        os: 'linux',
        arch: 'amd64',
      },

      steps: [
        {
          name: 'cache-restore',
          image: 'plugins/s3-cache:1',
          pull: true,
          settings: {
            endpoint: drone.from_secret('cache_s3_endpoint'),
            access_key: drone.from_secret('cache_s3_access_key'),
            secret_key: drone.from_secret('cache_s3_secret_key'),
            restore: true,
          },
        } + drone.whenOnline,
        {
          name: 'docs-deps',
          image: 'owncloudci/nodejs:11',
          pull: true,
          commands: [
            'cd docs/',
            'yarn install',
          ],
        },
        {
          name: 'docs-validate',
          image: 'owncloudci/nodejs:11',
          pull: true,
          commands: [
            'cd docs/',
            'yarn validate',
          ],
        },
        {
          name: 'docs-build',
          image: 'owncloudci/nodejs:11',
          pull: true,
          commands: [
            'cd docs/',
            'yarn antora',
          ],
        },
        {
          name: 'docs-pdf',
          image: 'owncloudci/asciidoctor:latest',
          pull: true,
          commands: [
            'cd docs/',
            'make pdf',
          ],
        },
        {
          name: 'docs-artifacts',
          image: 'owncloud/ubuntu:latest',
          pull: true,
          commands: [
            'tree docs/public/',
          ],
        },
        {
          name: 'cache-rebuild',
          image: 'plugins/s3-cache:1',
          pull: true,
          settings: {
            endpoint: drone.from_secret('cache_s3_endpoint'),
            access_key: drone.from_secret('cache_s3_access_key'),
            secret_key: drone.from_secret('cache_s3_secret_key'),
            rebuild: true,
            mount: 'docs/cache'
          },
        } + drone.whenOnline + drone.whenPush,

        {
          name: 'cache-flush',
          image: 'plugins/s3-cache:1',
          pull: true,
          settings: {
            endpoint: drone.from_secret('cache_s3_endpoint'),
            access_key: drone.from_secret('cache_s3_access_key'),
            secret_key: drone.from_secret('cache_s3_secret_key'),
            flush: true,
            flush_age: 14,
          },
        } + drone.whenOnline + drone.whenPush,

        {
          name: 'upload-pdf',
          image: 'plugins/s3-sync:1',
          pull: true,
          environment: {
            AWS_ACCESS_KEY_ID: drone.from_secret('aws_access_key_id'),
            AWS_SECRET_ACCESS_KEY: drone.from_secret('aws_secret_access_key'),
          },
          settings: {
            bucket: 'uploads',
            endpoint: 'https://doc.owncloud.com',
            path_style: true,
            source: 'docs/build/',
            target: '/deploy/',
            delete: false,
          },
        } + drone.whenOnline + drone.whenPush,
      ],
      trigger: {
        event: [
          'push',
          'pull_request',
          'tag',
        ],
      },
    },

  notification(depends_on=[], include_events=[], exclude_events=[])::
    {
      kind: 'pipeline',
      name: 'notifications',
      platform: {
        os: 'linux',
        arch: 'amd64',
      },
      clone: {
        disable: false,
      },
      steps: [
        {
          name: 'notification',
          image: 'plugins/slack',
          pull: true,
          settings: {
            webhook: drone.from_secret('slack_webhook'),
            channel: 'desktop',
          },
        },
      ],
      trigger: {
        status: [
          'success',
          'failure',
        ],
        event: {
          exclude: exclude_events,
          include: include_events,
        },
      },
      depends_on: depends_on,
    },
}
