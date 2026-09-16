---
kind: workflow
rows:
  - id: xa72q22kf6jt398mhaeb93p0
    name: Default
    project_id: pwd7f8n4egx222rkl7emmzrx
statuses:
  - id: a5jwqy149phnyd6a8zf6okcq
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: xa72q22kf6jt398mhaeb93p0
  - id: e6lv58a6uhxkwwffg9nqlse8
    name: Blocked
    category: TODO
    position: 4
    workflow_id: xa72q22kf6jt398mhaeb93p0
  - id: ilod0ubln4hm215wxhzorv0n
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: xa72q22kf6jt398mhaeb93p0
  - id: q46gyxv0i9sosny01adtjr42
    name: Done
    category: DONE
    position: 3
    workflow_id: xa72q22kf6jt398mhaeb93p0
  - id: v4nhf8t8xfixi16z4sy0naq2
    name: To Do
    category: TODO
    position: 0
    workflow_id: xa72q22kf6jt398mhaeb93p0
transitions:
  - id: c8sejlvpvj6f73q8bxlq787s
    name: Block
    position: 9
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: e6lv58a6uhxkwwffg9nqlse8
    from_status_id: null
  - id: h7f89dbf0v21hjp8dvey902a
    name: Start progress
    position: 0
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: ilod0ubln4hm215wxhzorv0n
    from_status_id: v4nhf8t8xfixi16z4sy0naq2
  - id: hbhfm0ylnay0qy4qndj6p5uq
    name: Back to progress
    position: 3
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: ilod0ubln4hm215wxhzorv0n
    from_status_id: a5jwqy149phnyd6a8zf6okcq
  - id: hv80uhwt319jwb1xww7nqpcl
    name: Unblock
    position: 7
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: ilod0ubln4hm215wxhzorv0n
    from_status_id: e6lv58a6uhxkwwffg9nqlse8
  - id: m7ettvuresiie7v7tbp7pj76
    name: Stop progress
    position: 1
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: v4nhf8t8xfixi16z4sy0naq2
    from_status_id: ilod0ubln4hm215wxhzorv0n
  - id: nd1j62vxi1d316myt7nnyukv
    name: Reopen
    position: 5
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: ilod0ubln4hm215wxhzorv0n
    from_status_id: q46gyxv0i9sosny01adtjr42
  - id: nf5tddpebtj7x03083qwi6o9
    name: Done
    position: 8
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: q46gyxv0i9sosny01adtjr42
    from_status_id: null
  - id: nlk34ez2lekre77ffgryqxkt
    name: Ready for review
    position: 2
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: a5jwqy149phnyd6a8zf6okcq
    from_status_id: ilod0ubln4hm215wxhzorv0n
  - id: xturnz3gpxd9dns6za8e03jr
    name: Reopen
    position: 4
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: v4nhf8t8xfixi16z4sy0naq2
    from_status_id: q46gyxv0i9sosny01adtjr42
  - id: xtvd1i7hxsg0ej1u2nvzev5n
    name: Unblock
    position: 6
    screen_id: null
    workflow_id: xa72q22kf6jt398mhaeb93p0
    to_status_id: v4nhf8t8xfixi16z4sy0naq2
    from_status_id: e6lv58a6uhxkwwffg9nqlse8
---
