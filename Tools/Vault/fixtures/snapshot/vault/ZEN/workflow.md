---
kind: workflow
rows:
  - id: vbrotodp2u5sl3hbekfejtl2
    name: Default
    project_id: r1m7idj9n6rkv5b1iulqz2ho
statuses:
  - id: dxhlckk5kgxv4mgp4e8lcu7s
    name: Done
    category: DONE
    position: 3
    workflow_id: vbrotodp2u5sl3hbekfejtl2
  - id: nif4xpysb3vxfbvcssw7s6q3
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: vbrotodp2u5sl3hbekfejtl2
  - id: q3spz9u36h2bf6qa942uqfz1
    name: To Do
    category: TODO
    position: 0
    workflow_id: vbrotodp2u5sl3hbekfejtl2
  - id: qowd8heaf6ej93d62zuugdxv
    name: Blocked
    category: TODO
    position: 4
    workflow_id: vbrotodp2u5sl3hbekfejtl2
  - id: rdei4coufefotrv2feispqhx
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: vbrotodp2u5sl3hbekfejtl2
transitions:
  - id: 1e236d20-54d9-40e9-822b-bd68470c7691
    name: Reopen
    position: 10
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: q3spz9u36h2bf6qa942uqfz1
    from_status_id: dxhlckk5kgxv4mgp4e8lcu7s
  - id: 4593b06c-0789-4f47-a2b1-21259abd0e79
    name: Done
    position: 8
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: dxhlckk5kgxv4mgp4e8lcu7s
    from_status_id: rdei4coufefotrv2feispqhx
  - id: 49ada904-5bba-4a20-859d-e2296fb4497b
    name: Back to progress
    position: 7
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: rdei4coufefotrv2feispqhx
    from_status_id: nif4xpysb3vxfbvcssw7s6q3
  - id: 7535964d-128f-4ebb-9b42-64af01c498d1
    name: Stop progress
    position: 4
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: q3spz9u36h2bf6qa942uqfz1
    from_status_id: rdei4coufefotrv2feispqhx
  - id: 75a9c629-4d95-40d6-a64f-678f7f3031d3
    name: Start progress
    position: 2
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: rdei4coufefotrv2feispqhx
    from_status_id: q3spz9u36h2bf6qa942uqfz1
  - id: 879c2a51-352d-46aa-860e-06802f877625
    name: Reopen
    position: 11
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: rdei4coufefotrv2feispqhx
    from_status_id: dxhlckk5kgxv4mgp4e8lcu7s
  - id: ab4f1be9-4ec6-4718-bcdb-30e26a0b19d4
    name: Unblock
    position: 14
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: rdei4coufefotrv2feispqhx
    from_status_id: qowd8heaf6ej93d62zuugdxv
  - id: ad35e1b7-266f-4d05-8bd6-c403ab9f645b
    name: Block
    position: 15
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: qowd8heaf6ej93d62zuugdxv
    from_status_id: null
  - id: b20eb949-2374-4e2c-997c-588fa30e3980
    name: Ready for review
    position: 6
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: nif4xpysb3vxfbvcssw7s6q3
    from_status_id: rdei4coufefotrv2feispqhx
  - id: bb046fb6-a200-4eb7-81c5-1105a9adeeac
    name: Unblock
    position: 12
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: q3spz9u36h2bf6qa942uqfz1
    from_status_id: qowd8heaf6ej93d62zuugdxv
  - id: c3bcb1b7-4ee2-4b68-971c-e99736905bc7
    name: Done
    position: 16
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: dxhlckk5kgxv4mgp4e8lcu7s
    from_status_id: null
  - id: dd793edd-306a-47ef-848f-00f18c49a52e
    name: Done
    position: 9
    screen_id: null
    workflow_id: vbrotodp2u5sl3hbekfejtl2
    to_status_id: dxhlckk5kgxv4mgp4e8lcu7s
    from_status_id: nif4xpysb3vxfbvcssw7s6q3
---
