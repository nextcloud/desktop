source(findFile('scripts', 'python/bdd.py'))

setupHooks('../shared/scripts/bdd_hooks.py')
collectStepDefinitions('./steps', '../shared/steps')


def main():
    testSettings.throwOnFailure = True
    runFeatureFile('test.feature')
