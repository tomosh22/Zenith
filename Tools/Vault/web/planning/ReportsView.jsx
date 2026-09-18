import React from 'react';
import { array, formatDate, numeric, positionOf, sprintId, sprintName, sprintStatus, text } from './helpers.js';

function reportData(project) {
  const reports = project?.reports && typeof project.reports === 'object' ? project.reports : {};
  const velocity = reports.velocity && typeof reports.velocity === 'object' ? reports.velocity : {};
  return {
    burndown: array(reports.burndown ?? reports.burndownPoints),
    burndownBySprint: reports.burndownBySprint ?? reports.burndowns ?? {},
    velocityRows: array(reports.velocityRows ?? velocity.rows ?? reports.velocity),
    average: numeric(reports.averageVelocity ?? velocity.average ?? reports.average, 0),
    epics: array(reports.epics ?? reports.epicProgress),
  };
}

function rowDate(row) {
  return text(row?.date ?? row?.day ?? row?.label);
}

function rowValue(row, keys) {
  for (const key of keys) if (row?.[key] !== undefined && row?.[key] !== null) return numeric(row[key]);
  return 0;
}

function BurndownChart({ points }) {
  const width = 720;
  const height = 240;
  const pad = { top: 18, right: 18, bottom: 32, left: 42 };
  const innerWidth = width - pad.left - pad.right;
  const innerHeight = height - pad.top - pad.bottom;
  const max = Math.max(1, ...points.flatMap(point => [rowValue(point, ['scope', 'committed']), rowValue(point, ['remaining', 'open']), rowValue(point, ['ideal'])]));
  const x = index => pad.left + (points.length <= 1 ? innerWidth / 2 : index / (points.length - 1) * innerWidth);
  const y = value => pad.top + innerHeight - value / max * innerHeight;
  const path = keys => points.map((point, index) => `${index ? 'L' : 'M'} ${x(index)} ${y(rowValue(point, keys))}`).join(' ');
  const last = points.at(-1);
  return <figure className="planning-chart-wrap"><svg className="planning-chart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label={`Burndown over ${points.length} days, ${rowValue(last, ['remaining', 'open'])} points remaining`}>
    <line x1={pad.left} y1={pad.top + innerHeight} x2={pad.left + innerWidth} y2={pad.top + innerHeight} className="planning-chart-axis" />
    <line x1={pad.left} y1={pad.top} x2={pad.left} y2={pad.top + innerHeight} className="planning-chart-axis" />
    <path d={path(['scope', 'committed'])} className="planning-chart-scope" />
    <path d={path(['ideal'])} className="planning-chart-ideal" />
    <path d={path(['remaining', 'open'])} className="planning-chart-remaining" />
    <text x={pad.left} y={pad.top - 4} className="planning-chart-label">{max} pts</text>
    <text x={pad.left} y={height - 8} className="planning-chart-label">{rowDate(points[0])}</text>
    <text x={pad.left + innerWidth} y={height - 8} textAnchor="end" className="planning-chart-label">{rowDate(last)}</text>
  </svg><figcaption><span className="planning-legend-remaining">— remaining</span><span className="planning-legend-scope">— scope</span><span>-- ideal</span></figcaption></figure>;
}

function VelocityBars({ rows, average }) {
  const max = Math.max(1, ...rows.flatMap(row => [rowValue(row, ['committed', 'planned']), rowValue(row, ['completed', 'done'])]));
  return <div className="planning-velocity"><p className="planning-report-copy">Average <strong>{average}</strong> points completed per sprint across {rows.length} completed {rows.length === 1 ? 'sprint' : 'sprints'}.</p><ul>{rows.map((row, index) => {
    const committed = rowValue(row, ['committed', 'planned', 'points']);
    const completed = rowValue(row, ['completed', 'done', 'donePoints']);
    return <li key={text(row?.sprintId ?? row?.id, `${index}`)}><span>{text(row?.sprintName ?? row?.sprint ?? row?.name, `Sprint ${index + 1}`)}</span><div className="planning-velocity-bars"><i title={`${committed} committed`} style={{ width: `${committed / max * 100}%` }} /><b title={`${completed} completed`} style={{ width: `${completed / max * 100}%` }} /></div><small>{completed}/{committed} pts</small></li>;
  })}</ul><div className="planning-velocity-legend"><span><i /> committed</span><span><b /> completed</span></div></div>;
}

function EpicReport({ row, index, onOpenIssue }) {
  const total = numeric(row?.total ?? row?.issues, 0);
  const done = numeric(row?.done ?? row?.completed, 0);
  const points = numeric(row?.points, 0);
  const donePoints = numeric(row?.donePoints ?? row?.completedPoints, 0);
  const percent = numeric(row?.percent, points > 0 ? Math.round(donePoints / points * 100) : total ? Math.round(done / total * 100) : 0);
  const key = row?.key ?? row?.issueKey;
  return <li><button type="button" onClick={() => key && onOpenIssue?.(key)} disabled={!key}><div><strong>{text(row?.key ?? row?.issueKey ?? row?.name, 'Epic')}</strong><span>{text(row?.title ?? row?.name)}</span><small>{done}/{total} done{points > 0 ? ` · ${donePoints}/${points} pts` : ''}</small></div><div className="planning-progress" aria-label={`${text(row?.title ?? row?.name, 'Epic')} progress`}><span style={{ width: `${Math.max(0, Math.min(100, percent))}%` }} /><small>{percent}%</small></div></button></li>;
}

export function ReportsView({ project, readOnly = false, onOpenIssue }) {
  void readOnly;
  const data = reportData(project);
  const sprints = array(project?.sprints).slice().sort((left, right) => positionOf(left?.position, 0) - positionOf(right?.position, 0) || sprintName(left).localeCompare(sprintName(right)));
  const [selected, setSelected] = React.useState('__active__');
  const burndown = React.useMemo(() => {
    if (selected !== '__active__') {
      const source = Array.isArray(data.burndownBySprint) ? data.burndownBySprint.find(item => sprintId(item) === selected)?.points : data.burndownBySprint?.[selected];
      // A selected sprint is an explicit scope. Do not silently chart the
      // current sprint when that sprint has no dated history of its own.
      return Array.isArray(source) ? source : [];
    }
    return data.burndown;
  }, [data, selected]);
  const selectedSprint = sprints.find(sprint => sprintId(sprint) === selected);
  const heading = selectedSprint ? sprintName(selectedSprint) : (sprints.find(sprint => sprintStatus(sprint) === 'ACTIVE') ? sprintName(sprints.find(sprint => sprintStatus(sprint) === 'ACTIVE')) : 'Current sprint');

  return <section className="planning-surface planning-reports" data-testid="planning-reports">
    <header className="planning-surface-header"><div><p className="planning-eyebrow">Planning</p><h2>Reports</h2><p className="planning-subtitle">Sprint history and delivery progress</p></div>{sprints.length > 0 && <label>Chart sprint <select aria-label="Charted sprint" value={selected} onChange={event => setSelected(event.target.value)}><option value="__active__">Current sprint</option>{sprints.map(sprint => <option key={sprintId(sprint)} value={sprintId(sprint)}>{sprintName(sprint)}{sprintStatus(sprint) === 'ACTIVE' ? ' (current)' : ''}</option>)}</select></label>}</header>
    <div className="planning-report-grid"><section className="planning-report-card" aria-labelledby="planning-burndown-heading"><h3 id="planning-burndown-heading">Sprint burndown{heading ? ` — ${heading}` : ''}</h3>{burndown.length ? <BurndownChart points={burndown} /> : <p className="planning-empty">No sprint with dated history is available to chart.</p>}</section><section className="planning-report-card" aria-labelledby="planning-velocity-heading"><h3 id="planning-velocity-heading">Velocity</h3>{data.velocityRows.length ? <VelocityBars rows={data.velocityRows} average={data.average} /> : <p className="planning-empty">No completed sprint history is available yet.</p>}</section></div>
    <section className="planning-report-card planning-epic-report" aria-labelledby="planning-epic-heading"><h3 id="planning-epic-heading">Epic progress</h3>{data.epics.length ? <ul>{data.epics.map((row, index) => <EpicReport key={text(row?.id ?? row?.key, `${index}`)} row={row} index={index} onOpenIssue={onOpenIssue} />)}</ul> : <p className="planning-empty">No epic report data is available.</p>}</section>
  </section>;
}

export const ProjectReports = ReportsView;
