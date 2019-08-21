local pipeline = import 'pipeline.libsonnet';

local translations_trigger = {
  cron: [ 'translations' ],
};

[
  # Build client and docs
  pipeline.build_and_test_client('gcc', 'g++', 'Release', 'Unix Makefiles'),
  pipeline.build_and_test_client('clang', 'clang++', 'Debug', 'Ninja'),
  pipeline.build_client_docs(),
  pipeline.notification(depends_on=[
    'gcc-release-make',
    'clang-debug-ninja',
    'build-docs',
  ]),

 # Sync translations
  pipeline.update_translations(
    'client',
    '2.6-l10n',
    'translations',
    read_image='rabits/qt:5.12-desktop',
    trigger=translations_trigger
  ),
  pipeline.update_translations(
    'nsis',
    '2.6-l10n',
    'admin/win/nsi/l10n',
    write_image='python:2.7-stretch',
    trigger=translations_trigger
  ),
  pipeline.notification(depends_on=[
    'translations-client',
    'translations-nsis'
  ]),
]
