---
kind: workflow
rows:
  - id: tjcebecalo498ru0asou6p6y
    name: Default
    project_id: ghzk5k8zytjebgeudf2wl3mj
statuses:
  - id: emv9uhqb881kljfnwto46w6i
    name: Blocked
    category: TODO
    position: 4
    workflow_id: tjcebecalo498ru0asou6p6y
  - id: exqkpzb74vqakjym8ako4u0t
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: tjcebecalo498ru0asou6p6y
  - id: pw8yvy2rea3kymnczfd5k1dn
    name: To Do
    category: TODO
    position: 0
    workflow_id: tjcebecalo498ru0asou6p6y
  - id: yz4pchqx0v94zttd8zkua8tx
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: tjcebecalo498ru0asou6p6y
  - id: z5vr8apgevk2ftk7adm34sir
    name: Done
    category: DONE
    position: 3
    workflow_id: tjcebecalo498ru0asou6p6y
transitions:
  - id: 08ef2afb-62bf-4b2d-96c5-bbe2554089f1
    name: Unblock
    position: 12
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: pw8yvy2rea3kymnczfd5k1dn
    from_status_id: emv9uhqb881kljfnwto46w6i
  - id: 09fd1ee1-48d8-413a-b3fd-ef8966310fdc
    name: Start progress
    position: 2
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: yz4pchqx0v94zttd8zkua8tx
    from_status_id: pw8yvy2rea3kymnczfd5k1dn
  - id: 34d8c343-d3bc-4e38-9b93-484ae0b77f42
    name: Ready for review
    position: 6
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: exqkpzb74vqakjym8ako4u0t
    from_status_id: yz4pchqx0v94zttd8zkua8tx
  - id: 3fa9b7d7-9a17-482e-bc98-ff8a27a0d0b8
    name: Done
    position: 16
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: z5vr8apgevk2ftk7adm34sir
    from_status_id: null
  - id: 88be4450-3844-4cc8-90ba-3248116d3b17
    name: Reopen
    position: 10
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: pw8yvy2rea3kymnczfd5k1dn
    from_status_id: z5vr8apgevk2ftk7adm34sir
  - id: b60280eb-5ef6-47c4-af50-a85dd79d00cd
    name: Reopen
    position: 11
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: yz4pchqx0v94zttd8zkua8tx
    from_status_id: z5vr8apgevk2ftk7adm34sir
  - id: b77952a3-9538-482e-aafb-1b9ed7ad5347
    name: Stop progress
    position: 4
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: pw8yvy2rea3kymnczfd5k1dn
    from_status_id: yz4pchqx0v94zttd8zkua8tx
  - id: bb9c6553-1b0f-4513-941c-1b83981e2fcb
    name: Done
    position: 8
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: z5vr8apgevk2ftk7adm34sir
    from_status_id: yz4pchqx0v94zttd8zkua8tx
  - id: bbe7d7ba-1d7b-4bf6-80b1-e85bf9fed656
    name: Unblock
    position: 14
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: yz4pchqx0v94zttd8zkua8tx
    from_status_id: emv9uhqb881kljfnwto46w6i
  - id: bffdbf53-d5b3-4074-b77c-ec7a747909db
    name: Back to progress
    position: 7
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: yz4pchqx0v94zttd8zkua8tx
    from_status_id: exqkpzb74vqakjym8ako4u0t
  - id: c95c2964-7877-414d-8660-6187d9a425d0
    name: Done
    position: 9
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: z5vr8apgevk2ftk7adm34sir
    from_status_id: exqkpzb74vqakjym8ako4u0t
  - id: ec903cf1-16f6-4926-9ab7-c2c452323392
    name: Block
    position: 15
    screen_id: null
    workflow_id: tjcebecalo498ru0asou6p6y
    to_status_id: emv9uhqb881kljfnwto46w6i
    from_status_id: null
---
