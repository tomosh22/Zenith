---
kind: workflow
rows:
  - id: s57xzyvddykdip2uzvcgur8f
    name: Default
    project_id: tq569u5dcunf9nu8gq42l94b
statuses:
  - id: ezlvsbwh7t6klvoaq9ckrq5r
    name: Blocked
    category: TODO
    position: 4
    workflow_id: s57xzyvddykdip2uzvcgur8f
  - id: ms4zum0hqqw1i1xc1uq60ndt
    name: Done
    category: DONE
    position: 3
    workflow_id: s57xzyvddykdip2uzvcgur8f
  - id: pqsd3pcjqpxlcty6k7gpz2os
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: s57xzyvddykdip2uzvcgur8f
  - id: u8gq4ql148xw7fjigrzwvj5h
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: s57xzyvddykdip2uzvcgur8f
  - id: v18scjxmdn6cgyzchs3rpcar
    name: To Do
    category: TODO
    position: 0
    workflow_id: s57xzyvddykdip2uzvcgur8f
transitions:
  - id: algvhc7vl4yencoukw281bur
    name: Stop progress
    position: 1
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: v18scjxmdn6cgyzchs3rpcar
    from_status_id: u8gq4ql148xw7fjigrzwvj5h
  - id: iiughzk85rtpl4e9ab30v233
    name: Done
    position: 8
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: ms4zum0hqqw1i1xc1uq60ndt
    from_status_id: null
  - id: l71clvaulipiit4ymood6spl
    name: Back to progress
    position: 3
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: u8gq4ql148xw7fjigrzwvj5h
    from_status_id: pqsd3pcjqpxlcty6k7gpz2os
  - id: qqf02th9o1g8u5p9p3i0pldy
    name: Start progress
    position: 0
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: u8gq4ql148xw7fjigrzwvj5h
    from_status_id: v18scjxmdn6cgyzchs3rpcar
  - id: s18ldb6gv8qm4qp4cc6kqecs
    name: Reopen
    position: 5
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: u8gq4ql148xw7fjigrzwvj5h
    from_status_id: ms4zum0hqqw1i1xc1uq60ndt
  - id: tlkrn0rwqv57254wtomjpd62
    name: Reopen
    position: 4
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: v18scjxmdn6cgyzchs3rpcar
    from_status_id: ms4zum0hqqw1i1xc1uq60ndt
  - id: tz70af3zyg3wu7orokxkmcj4
    name: Unblock
    position: 6
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: v18scjxmdn6cgyzchs3rpcar
    from_status_id: ezlvsbwh7t6klvoaq9ckrq5r
  - id: uau2dt3p5ca8suvo4102b394
    name: Block
    position: 9
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: ezlvsbwh7t6klvoaq9ckrq5r
    from_status_id: null
  - id: upn9jiwdwxsa8v79tkxi6zac
    name: Unblock
    position: 7
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: u8gq4ql148xw7fjigrzwvj5h
    from_status_id: ezlvsbwh7t6klvoaq9ckrq5r
  - id: ytg8qf7434t78939zt3boxkx
    name: Ready for review
    position: 2
    screen_id: null
    workflow_id: s57xzyvddykdip2uzvcgur8f
    to_status_id: pqsd3pcjqpxlcty6k7gpz2os
    from_status_id: u8gq4ql148xw7fjigrzwvj5h
---
