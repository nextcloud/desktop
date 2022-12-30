import urllib.request
import json


def executeStepThroughMiddleware(context, step):
    body = {"step": step}
    if hasattr(context, "table"):
        body["table"] = context.table

    params = json.dumps(body).encode('utf8')

    req = urllib.request.Request(
        context.userData['middlewareUrl'] + 'execute',
        data=params,
        headers={"Content-Type": "application/json"},
        method='POST',
    )
    try:
        urllib.request.urlopen(req)
    except urllib.error.HTTPError as e:
        raise Exception(
            "Step execution through test middleware failed. Error: " + e.read().decode()
        )


@Given(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Given " + stepPart1 + " " + stepPart2)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Given(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Given " + stepPart1)
    global usersDataFromMiddleware
    usersDataFromMiddleware = None


@Then(r"^(.*) on the server (.*)$", regexp=True)
def step(context, stepPart1, stepPart2):
    executeStepThroughMiddleware(context, "Then " + stepPart1 + " " + stepPart2)


@Then(r"^(.*) on the server$", regexp=True)
def step(context, stepPart1):
    executeStepThroughMiddleware(context, "Then " + stepPart1)
