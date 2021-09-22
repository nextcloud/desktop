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

                            for error in test_step['tests']:

                                # Sometimes, the step with assertions operations also contains this "tests" object.
                                # And we do not consider it to be an error, if there is a result key with value "PASS"
                                if 'result' in error and error['result'] == 'PASS':
                                    continue

                                # Append the information of failing tests into the list of failing tests
                                # If the error detail is missing(occurs mainly in runtime error) then we display the entire error object.
                                test = {
                                    "Feature File": str(feature_file['name']),
                                    "Feature": str(feature['name']),
                                    "Scenario": str(scenario['name']),
                                    "Test Step": str(test_step['name']),
                                    "Error Details": str(error['detail'])
                                    if ('detail' in error)
                                    else "Error details not found",
                                }

                                failing_tests.append(test)

    return failing_tests


def filter_redundancy(raw_data):
    unique_scenarios = []
    filtered_data = []

    for scenario in raw_data:
        if scenario['Scenario'] not in unique_scenarios:
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
