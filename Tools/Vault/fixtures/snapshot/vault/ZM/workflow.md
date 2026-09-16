---
kind: workflow
rows:
  - id: ca7th64zozqf5iryluq0dq09
    name: Default
    project_id: n5y2i7n3yn436t00r2v41s82
statuses:
  - id: enehe6xvic4js4sq8lff0zot
    name: Done
    category: DONE
    position: 3
    workflow_id: ca7th64zozqf5iryluq0dq09
  - id: j4j0v44p666rcqfinfjgsq9a
    name: To Do
    category: TODO
    position: 0
    workflow_id: ca7th64zozqf5iryluq0dq09
  - id: jbybxbf31nhmpzefxmp6agd1
    name: Blocked
    category: TODO
    position: 4
    workflow_id: ca7th64zozqf5iryluq0dq09
  - id: x9t78vfd3ti6ucfpo98ztv1i
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: ca7th64zozqf5iryluq0dq09
  - id: z2jh81hyl7grezblj065ovg0
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: ca7th64zozqf5iryluq0dq09
transitions:
  - id: 2f0d11a7-af81-4c0a-8a4b-80d030a38654
    name: Reopen
    position: 11
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: x9t78vfd3ti6ucfpo98ztv1i
    from_status_id: enehe6xvic4js4sq8lff0zot
  - id: 31e53879-b551-4a7a-b6bb-ca2ce901b893
    name: Back to progress
    position: 7
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: x9t78vfd3ti6ucfpo98ztv1i
    from_status_id: z2jh81hyl7grezblj065ovg0
  - id: 376e64db-6011-4d42-a0f2-39e7e434b675
    name: Done
    position: 9
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: enehe6xvic4js4sq8lff0zot
    from_status_id: z2jh81hyl7grezblj065ovg0
  - id: 7c319abb-05b6-4d84-87c7-f5c47b16cf67
    name: Done
    position: 16
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: enehe6xvic4js4sq8lff0zot
    from_status_id: null
  - id: 883721cf-cad6-4cde-8f43-751f20764209
    name: Reopen
    position: 10
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: j4j0v44p666rcqfinfjgsq9a
    from_status_id: enehe6xvic4js4sq8lff0zot
  - id: a4a5c430-dd4b-43b8-aeac-0d7c5d5f3cd1
    name: Unblock
    position: 14
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: x9t78vfd3ti6ucfpo98ztv1i
    from_status_id: jbybxbf31nhmpzefxmp6agd1
  - id: c27513d7-122d-4cca-81fb-c1d8616ffbd9
    name: Unblock
    position: 12
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: j4j0v44p666rcqfinfjgsq9a
    from_status_id: jbybxbf31nhmpzefxmp6agd1
  - id: ccb22e48-9ef9-466e-b048-38ea9db516e2
    name: Block
    position: 15
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: jbybxbf31nhmpzefxmp6agd1
    from_status_id: null
  - id: ce09845a-4d7b-48d6-98c7-a46cbb558757
    name: Done
    position: 8
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: enehe6xvic4js4sq8lff0zot
    from_status_id: x9t78vfd3ti6ucfpo98ztv1i
  - id: e4833a3b-43ba-4a60-b910-86750e9335b5
    name: Ready for review
    position: 6
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: z2jh81hyl7grezblj065ovg0
    from_status_id: x9t78vfd3ti6ucfpo98ztv1i
  - id: ee8bc065-7708-4410-93e7-578c2dd8ee78
    name: Stop progress
    position: 4
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: j4j0v44p666rcqfinfjgsq9a
    from_status_id: x9t78vfd3ti6ucfpo98ztv1i
  - id: fca88311-b925-423d-b74c-29426786734b
    name: Start progress
    position: 2
    screen_id: null
    workflow_id: ca7th64zozqf5iryluq0dq09
    to_status_id: x9t78vfd3ti6ucfpo98ztv1i
    from_status_id: j4j0v44p666rcqfinfjgsq9a
---
