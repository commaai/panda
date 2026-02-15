the HTIL jenkins CI got flaky. your goal is to make our jenkins HITL tests pass 50 times in a row!
do not stop until it's done. you can push, commit, and check results as often as you want on this branch.

here's an example jenkins URL: https://jenkins.comma.life/blue/organizations/jenkins/panda/detail/reduce-power/1/pipeline.

DO NOT write hacks to make this happen, such as generic test retries.
we are looking for good root causes and principled fixes.

to trigger runs without changing code, you can use the jenkins API
