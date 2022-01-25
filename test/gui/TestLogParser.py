# # Input parameters
# name of the file that contains the data to be parsed
import json
import sys

# Method that tries to find the data using nested loops
def traverse_loop(data):
    failing_tests = []

    # Loop level 0 where we loop through all the feature files inside the only test suites that we have.
    for feature_file in data["tests"][0]["tests"]:

        # Loop through all the features in the feature file
        for feature in feature_file['tests']:

            # Loop through all the scenarios in the feature
            for scenario in feature['tests']:

                # If the scenario is not skipped, then loop through all the steps in the scenario
                if "tests" in scenario:

                    for test_step in scenario['tests']:

                        # If the test step fails then it contains further "tests" object
                        # So loop through all the errors in the test step
                        if "tests" in test_step:

                            for test_log in test_step['tests']:

                                # Sometimes, the step with assertions operations also contains this "tests" object.
                                # And we do not consider it to be an error, if there is a result key with value "PASS"
                                if (
                                    'result' in test_log
                                    and test_log['result'] == 'PASS'
                                ):
                                    continue

                                # Again, we will have to loop through the 'tests' to get failing tests from Scenario Outlines.
                                if "tests" in test_log and len(test_log['tests']) > 0:
                                    for outlineStep in test_log['tests']:
                                        # Append the information of failing tests into the list of failing tests
                                        if 'result' in outlineStep and (
                                            outlineStep['result'] == 'ERROR'
                                            or outlineStep['result'] == 'FAIL'
                                        ):
                                            failing_test = {
                                                "Feature File": str(
                                                    feature_file['name']
                                                ),
                                                "Feature": str(feature['name']),
                                                "Scenario": str(scenario['name']),
                                                "Example": str(test_step['name']),
                                                "Test Step": str(test_log['name']),
                                                "Error Details": str(
                                                    outlineStep['detail']
                                                )
                                                if ('detail' in outlineStep)
                                                else "Error details not found",
                                            }
                                            failing_tests.append(failing_test)
                                    continue

                                # Append the information of failing tests into the list of failing tests
                                # If the error detail is missing(occurs mainly in runtime error) then we display "Error details not found" message.
                                if 'result' in test_log and (
                                    test_log['result'] == 'ERROR'
                                    or test_log['result'] == 'FAIL'
                                ):
                                    failing_test = {
                                        "Feature File": str(feature_file['name']),
                                        "Feature": str(feature['name']),
                                        "Scenario": str(scenario['name']),
                                        "Test Step": str(test_step['name']),
                                        "Error Details": str(test_log['detail'])
                                        if ('detail' in test_log)
                                        else "Error details not found",
                                    }
                                    failing_tests.append(failing_test)

    return failing_tests


def filter_redundancy(raw_data):
    unique_scenarios = []
    unique_scenario_outline_examples = []
    filtered_data = []

    for scenario in raw_data:
        # The 'Scenario' name in Scenario Outline is not unique for each test.
        # So we use 'Example' to uniquely identify each test in Scenario Outline.
        if (
            'Example' in scenario
            and scenario['Example'] not in unique_scenario_outline_examples
        ):
            unique_scenario_outline_examples.append(scenario['Example'])
            filtered_data.append(scenario)
        elif 'Example' not in scenario and scenario['Scenario'] not in unique_scenarios:
            unique_scenarios.append(scenario['Scenario'])
            filtered_data.append(scenario)
        else:
            if scenario['Error Details'] != "Error details not found":
                for repeated_scenario in filtered_data:
                    if repeated_scenario['Scenario'] == scenario['Scenario']:
                        repeated_scenario['Error Details'] = scenario['Error Details']

    return filtered_data


f = open(str(sys.argv[1]))

# returns JSON object as a dictionary
data = json.load(f)

# Traverse through the json file to look for failing tests
failing_tests_raw = traverse_loop(data)

# Remove duplicate nodes, if exists
# This step is necessary because sometimes the data in failing_tests_raw is redundant.
failing_tests = filter_redundancy(failing_tests_raw)

print(json.dumps(failing_tests, indent=4, sort_keys=True))

f.close()
