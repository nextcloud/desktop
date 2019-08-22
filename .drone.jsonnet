#
# We are building GCC with make and Clang with ninja, the combinations are more
# or less arbitrarily chosen. We just want to check that both compilers and both
# CMake generators work. It's unlikely that a specific generator only breaks
# with a specific compiler.
#

local pipeline = import 'pipeline.libsonnet';

local translations_trigger = {
  cron: [ 'translations' ],
};

[
  # Build client and docs
  pipeline.build_and_test_client('gcc', 'g++', 'Release', 'Unix Makefiles'),
  pipeline.build_and_test_client('clang', 'clang++', 'Debug', 'Ninja'),
  pipeline.build_client_docs(),
  pipeline.notification(
    name='build',
    depends_on=[
      'gcc-release-make',
      'clang-debug-ninja',
      'build-docs',
    ]
  ),

 # Sync translations
  pipeline.update_translations(
    'client',
    'translations',
    read_image='rabits/qt:5.12-desktop',
    trigger=translations_trigger
  ),
  pipeline.update_translations(
    'nsis',
    'admin/win/nsi/l10n',
    write_image='python:2.7-stretch',
    trigger=translations_trigger,
    depends_on=['translations-client'], // needs to run after translations-client because drone-git-push does not rebase before pushing
  ),
  pipeline.notification(
    name='translations',
    trigger=translations_trigger,
    depends_on=[
      'translations-client',
      'translations-nsis'
    ],
  ),
]
