---
kind: workflow
rows:
  - id: t1uflo0uj1shgk0guvvdjjnc
    name: Default
    project_id: f1ecqdf4ab8xgdorovsz6t7v
statuses:
  - id: bf7hz0zv82i50xy4zpqzplg0
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
  - id: c97ycrwolc2sj3b8g9yzde50
    name: Done
    category: DONE
    position: 3
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
  - id: ekfhwq3kkne8l2w5jrfagv13
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
  - id: lrby3rinime7pp20c42qpvtg
    name: To Do
    category: TODO
    position: 0
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
  - id: oz2l2blqsvn3bfs6nzrzgvxp
    name: Blocked
    category: TODO
    position: 4
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
transitions:
  - id: 371ad11b-03a5-4367-9756-3806085c69e9
    name: Done
    position: 8
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: c97ycrwolc2sj3b8g9yzde50
    from_status_id: bf7hz0zv82i50xy4zpqzplg0
  - id: 512f6479-3f14-494f-9f6b-425bbbbdd1ff
    name: Reopen
    position: 10
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: lrby3rinime7pp20c42qpvtg
    from_status_id: c97ycrwolc2sj3b8g9yzde50
  - id: 5b0b1a14-a0d4-4db6-bd45-3bf7b5a99952
    name: Back to progress
    position: 7
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: bf7hz0zv82i50xy4zpqzplg0
    from_status_id: ekfhwq3kkne8l2w5jrfagv13
  - id: 694dd7c9-d74c-46b7-9dad-6d747df6a05c
    name: Unblock
    position: 14
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: bf7hz0zv82i50xy4zpqzplg0
    from_status_id: oz2l2blqsvn3bfs6nzrzgvxp
  - id: 97e086bb-66fe-4318-bbda-a197b5d43272
    name: Unblock
    position: 12
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: lrby3rinime7pp20c42qpvtg
    from_status_id: oz2l2blqsvn3bfs6nzrzgvxp
  - id: b1ca336f-4a17-445d-8040-70f23e06b3a6
    name: Reopen
    position: 11
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: bf7hz0zv82i50xy4zpqzplg0
    from_status_id: c97ycrwolc2sj3b8g9yzde50
  - id: b25b4474-f7be-49c7-81a6-77e75015335a
    name: Done
    position: 9
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: c97ycrwolc2sj3b8g9yzde50
    from_status_id: ekfhwq3kkne8l2w5jrfagv13
  - id: b767bd3f-62af-4671-9e4a-4b98c3a721ee
    name: Start progress
    position: 2
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: bf7hz0zv82i50xy4zpqzplg0
    from_status_id: lrby3rinime7pp20c42qpvtg
  - id: bbe57960-a50c-4688-a758-adb5421e3f0e
    name: Block
    position: 15
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: oz2l2blqsvn3bfs6nzrzgvxp
    from_status_id: null
  - id: d6b94e75-7687-44b1-a2c2-16f29612bd8c
    name: Stop progress
    position: 4
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: lrby3rinime7pp20c42qpvtg
    from_status_id: bf7hz0zv82i50xy4zpqzplg0
  - id: f6c858e5-080a-41bf-8247-853f70571427
    name: Done
    position: 16
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: c97ycrwolc2sj3b8g9yzde50
    from_status_id: null
  - id: feb26b8a-33a6-4386-9f04-311052ee7595
    name: Ready for review
    position: 6
    screen_id: null
    workflow_id: t1uflo0uj1shgk0guvvdjjnc
    to_status_id: ekfhwq3kkne8l2w5jrfagv13
    from_status_id: bf7hz0zv82i50xy4zpqzplg0
---
