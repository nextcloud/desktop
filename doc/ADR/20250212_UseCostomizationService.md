# Use Customization Service

## Status

accepted

## Context

Nextcloud (NC) offers a customization service. The service is a Nextcloud white-labeling offering. It allows customers and their forks to be redesigned according to their own specifications. There are two levels.

### Brander

Here, the icons, names, and technical aspects are being adjusted to release a simply redesigned, color-enhanced Nextcloud.

### Actual Customization Service

This will integrate deeper changes into the branded client. This could be anything, in principle.

For this to work we need to create Pull-Requests against the branches of the nextcloud repo. These pull requests can not depend on the same code line.

More information can be read [here](https://portal.nextcloud.com/article/Branding/Customization-Service)

### Workflow on the Repo

A PR always has a source branch and a target branch. The target branch must be `master` in our case. Source branches can follow our established workflow. However, there are some conditions:

- Concurrent work on different features must be possible.
- Local builds must be possible for testing purposes.
- A nightly/onPush build might be desirable.
- It must be ensured that everyone has an up-to-date status with all PRs.

There are two possible Solutions:

1. We use one Pull-Request containing all changes.
2. We use multiple Pull requests containing changes grouped by files.

## Decision

We will use only one PR containing all our changes.

At this point in time, it would be too much work to split all Changes into separate branches/PRs.

## Implementation Details

### Branching

- We use the master branch only for pulling the updated nc/master branch.
- We have a separate develop branch to develop changes.
- We create a PR to nc/masetr from develop.

## Consequences

### Positive

- We can use our existing git-workflow
- We have a simple repo-structure with only 2 really needed branches
- It is easy to build locally, because all changes are on one branch
- Less Branches to backport
- We do not need to sort every change to a separate PR

### Negative

- Merge conflicts can be complex
- The PR will contain all the changes, this could be overwhelming to check

### Further Thoughts

- We need to see how this works in practice
- This is not final