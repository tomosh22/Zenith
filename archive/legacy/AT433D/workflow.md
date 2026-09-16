---
kind: workflow
rows:
  - id: tfyp9drqhue1t4y734l1j63h
    name: Default
    project_id: h5j398ck1xxlfeatqa21d4sc
statuses:
  - id: e3beigarow9vj73ducl8jwm6
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: tfyp9drqhue1t4y734l1j63h
  - id: grdk49lyc93cuedascnbi24a
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: tfyp9drqhue1t4y734l1j63h
  - id: hov118u1pxcm8b7zs0ezk4ap
    name: Blocked
    category: TODO
    position: 4
    workflow_id: tfyp9drqhue1t4y734l1j63h
  - id: i05hdgk4xetky7dy4c5998i3
    name: To Do
    category: TODO
    position: 0
    workflow_id: tfyp9drqhue1t4y734l1j63h
  - id: lylbgmqrhdq3omm8gmsiyoo2
    name: Done
    category: DONE
    position: 3
    workflow_id: tfyp9drqhue1t4y734l1j63h
transitions:
  - id: aazwskx2i65uswx4eus9hqnw
    name: Stop progress
    position: 1
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: i05hdgk4xetky7dy4c5998i3
    from_status_id: e3beigarow9vj73ducl8jwm6
  - id: bofvb9kt4m7c1mjwa2i096d9
    name: Back to progress
    position: 3
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: e3beigarow9vj73ducl8jwm6
    from_status_id: grdk49lyc93cuedascnbi24a
  - id: ed0uboogf1taipd2l6q20xwp
    name: Block
    position: 9
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: hov118u1pxcm8b7zs0ezk4ap
    from_status_id: null
  - id: kd5uojojxk7xv0qiypcxbvyd
    name: Unblock
    position: 6
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: i05hdgk4xetky7dy4c5998i3
    from_status_id: hov118u1pxcm8b7zs0ezk4ap
  - id: oaokxpb9z7j933y1d14ptjja
    name: Done
    position: 8
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: lylbgmqrhdq3omm8gmsiyoo2
    from_status_id: null
  - id: rnyi3lwr4foldkb0oaeg6w1p
    name: Unblock
    position: 7
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: e3beigarow9vj73ducl8jwm6
    from_status_id: hov118u1pxcm8b7zs0ezk4ap
  - id: s5klerf4tyddltyz03672o6x
    name: Ready for review
    position: 2
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: grdk49lyc93cuedascnbi24a
    from_status_id: e3beigarow9vj73ducl8jwm6
  - id: sljmuw37a0u172ljap41scrv
    name: Start progress
    position: 0
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: e3beigarow9vj73ducl8jwm6
    from_status_id: i05hdgk4xetky7dy4c5998i3
  - id: usbdvvit77q9h87y5rb3g6dq
    name: Reopen
    position: 4
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: i05hdgk4xetky7dy4c5998i3
    from_status_id: lylbgmqrhdq3omm8gmsiyoo2
  - id: xypszjxoao4m2r7t254y3unf
    name: Reopen
    position: 5
    screen_id: null
    workflow_id: tfyp9drqhue1t4y734l1j63h
    to_status_id: e3beigarow9vj73ducl8jwm6
    from_status_id: lylbgmqrhdq3omm8gmsiyoo2
---
