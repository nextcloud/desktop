local pipeline = import 'pipeline.libsonnet';

[
  pipeline.build_and_test_client('gcc', 'g++', 'Release', 'Unix Makefiles'),
  pipeline.build_and_test_client('clang', 'clang++', 'Debug', 'Ninja'),
  pipeline.build_client_docs(),
  pipeline.notification(depends_on=[
    'gcc-release-make',
    'clang-debug-ninja',
    'build-docs',
  ]),
]
