import React from 'react';
import { BacklogView } from './BacklogView.jsx';
import { BoardView } from './BoardView.jsx';
import { ReleasesView } from './ReleasesView.jsx';
import { ReportsView } from './ReportsView.jsx';
import { RoadmapView } from './RoadmapView.jsx';
import './planning.css';

const views = {
  board: BoardView,
  backlog: BacklogView,
  roadmap: RoadmapView,
  releases: ReleasesView,
  reports: ReportsView,
};

export function PlanningWorkspace({ section = 'board', project, api, onChanged, onOpenIssue, onCreateIssue, readOnly = false }) {
  const View = views[section] ?? BoardView;
  return <View section={section} project={project} api={api} onChanged={onChanged} onOpenIssue={onOpenIssue} onCreateIssue={onCreateIssue} readOnly={readOnly} />;
}

export { BoardView, BacklogView, RoadmapView, ReleasesView, ReportsView };
export { ProjectBoard } from './BoardView.jsx';
export { ProjectBacklog } from './BacklogView.jsx';
export { ProjectRoadmap } from './RoadmapView.jsx';
export { ProjectReleases } from './ReleasesView.jsx';
export { ProjectReports } from './ReportsView.jsx';

export default PlanningWorkspace;

