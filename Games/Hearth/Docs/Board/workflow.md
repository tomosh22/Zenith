---
kind: workflow
rows:
  - id: mc0jtshizc3qveflre6zpu9w
    name: Default
    project_id: vhoct5awix6pbnjxl0qub2ae
statuses:
  - id: b0xk1276glbntckwuve2i0oq
    name: In Progress
    category: IN_PROGRESS
    position: 1
    workflow_id: mc0jtshizc3qveflre6zpu9w
  - id: e7e1gq5gtijb1rytgyv5fmdl
    name: In Review
    category: IN_PROGRESS
    position: 2
    workflow_id: mc0jtshizc3qveflre6zpu9w
  - id: hmcx3rqugjnu4r3n5h9jkq7x
    name: Done
    category: DONE
    position: 3
    workflow_id: mc0jtshizc3qveflre6zpu9w
  - id: hmhweafbdrufyjvy0wohqebd
    name: To Do
    category: TODO
    position: 0
    workflow_id: mc0jtshizc3qveflre6zpu9w
  - id: kmnbfeem2tz8a07nnwoahb34
    name: Blocked
    category: TODO
    position: 4
    workflow_id: mc0jtshizc3qveflre6zpu9w
transitions:
  - id: fyobrerlza9ofcmtrcfrec8s
    name: Block
    position: 9
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: kmnbfeem2tz8a07nnwoahb34
    from_status_id: null
  - id: h1mvrawetxxb3mt71nan0175
    name: Unblock
    position: 7
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: b0xk1276glbntckwuve2i0oq
    from_status_id: kmnbfeem2tz8a07nnwoahb34
  - id: hv6mbcm3xbcl2dqu45but0id
    name: Unblock
    position: 6
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: hmhweafbdrufyjvy0wohqebd
    from_status_id: kmnbfeem2tz8a07nnwoahb34
  - id: iv6tjdfoaubo3qkp5vhfvjvv
    name: Ready for review
    position: 2
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: e7e1gq5gtijb1rytgyv5fmdl
    from_status_id: b0xk1276glbntckwuve2i0oq
  - id: jk8baoci25ci4dajjhju490o
    name: Back to progress
    position: 3
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: b0xk1276glbntckwuve2i0oq
    from_status_id: e7e1gq5gtijb1rytgyv5fmdl
  - id: nche7if9m5v2qtwhzw0d3l7o
    name: Start progress
    position: 0
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: b0xk1276glbntckwuve2i0oq
    from_status_id: hmhweafbdrufyjvy0wohqebd
  - id: nm9ijqok24z2o1c0hvtq0er9
    name: Done
    position: 8
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: hmcx3rqugjnu4r3n5h9jkq7x
    from_status_id: null
  - id: nw81nnxunhsx2jqtit6i4vxt
    name: Reopen
    position: 4
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: hmhweafbdrufyjvy0wohqebd
    from_status_id: hmcx3rqugjnu4r3n5h9jkq7x
  - id: w0fb5wny5al3qcn643ktv1zm
    name: Stop progress
    position: 1
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: hmhweafbdrufyjvy0wohqebd
    from_status_id: b0xk1276glbntckwuve2i0oq
  - id: zdwimvq6edyna8s4ch1573gu
    name: Reopen
    position: 5
    screen_id: null
    workflow_id: mc0jtshizc3qveflre6zpu9w
    to_status_id: b0xk1276glbntckwuve2i0oq
    from_status_id: hmcx3rqugjnu4r3n5h9jkq7x
---
