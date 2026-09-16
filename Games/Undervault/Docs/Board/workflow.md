---
kind: workflow
rows:
  - id: cojf1y0m5j5yswpt43c6jhru
    name: Default
    project_id: u0wm3n3u0d5a9s9fjyx9nmg5
statuses:
  - id: db59zn40dl2qfzm6im0ykjyk
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: cojf1y0m5j5yswpt43c6jhru
  - id: dfh8mpq7dr7lgdu5tmt1kcc9
    name: Blocked
    category: TODO
    position: 4
    workflow_id: cojf1y0m5j5yswpt43c6jhru
  - id: dv0cr92qbmdjgc6jz9irhnni
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: cojf1y0m5j5yswpt43c6jhru
  - id: e73nnohbzihswjrx0y52pwoz
    name: Done
    category: DONE
    position: 3
    workflow_id: cojf1y0m5j5yswpt43c6jhru
  - id: musu2v1chmhw2lgcn1edbi29
    name: To Do
    category: TODO
    position: 0
    workflow_id: cojf1y0m5j5yswpt43c6jhru
transitions:
  - id: eadeloi17li6hs6y9xdafioh
    name: Stop progress
    position: 1
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: musu2v1chmhw2lgcn1edbi29
    from_status_id: db59zn40dl2qfzm6im0ykjyk
  - id: ig69mc9xg3aftuaqwyho63bz
    name: Done
    position: 8
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: e73nnohbzihswjrx0y52pwoz
    from_status_id: null
  - id: ii7x2vw5fjzk23qpyjtqgf8x
    name: Back to progress
    position: 3
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: db59zn40dl2qfzm6im0ykjyk
    from_status_id: dv0cr92qbmdjgc6jz9irhnni
  - id: jmb232byg6v77fd3knqyrm7y
    name: Unblock
    position: 6
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: musu2v1chmhw2lgcn1edbi29
    from_status_id: dfh8mpq7dr7lgdu5tmt1kcc9
  - id: nl3uuq37fvnm15xlv0b04w7c
    name: Block
    position: 9
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: dfh8mpq7dr7lgdu5tmt1kcc9
    from_status_id: null
  - id: ov27stiow660cgc3n8ba1o2u
    name: Reopen
    position: 5
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: db59zn40dl2qfzm6im0ykjyk
    from_status_id: e73nnohbzihswjrx0y52pwoz
  - id: qudv0oaa0s141tkpkjm0xmdc
    name: Reopen
    position: 4
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: musu2v1chmhw2lgcn1edbi29
    from_status_id: e73nnohbzihswjrx0y52pwoz
  - id: saqj8pbp74wt638qv24upcn8
    name: Unblock
    position: 7
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: db59zn40dl2qfzm6im0ykjyk
    from_status_id: dfh8mpq7dr7lgdu5tmt1kcc9
  - id: tvb8ipc2hj4sn74sojbdzx7j
    name: Ready for review
    position: 2
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: dv0cr92qbmdjgc6jz9irhnni
    from_status_id: db59zn40dl2qfzm6im0ykjyk
  - id: y78doqeck2cpk3qesraumdt2
    name: Start progress
    position: 0
    screen_id: null
    workflow_id: cojf1y0m5j5yswpt43c6jhru
    to_status_id: db59zn40dl2qfzm6im0ykjyk
    from_status_id: musu2v1chmhw2lgcn1edbi29
---
