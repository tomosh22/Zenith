import React from 'react';
import { WorkflowSettings } from './WorkflowSettings.jsx';
import { FieldsSettings } from './FieldsSettings.jsx';
import { BoardSettings } from './BoardSettings.jsx';
import './settings.css';

const SETTINGS_SECTIONS = {
  workflow: WorkflowSettings,
  fields: FieldsSettings,
  'board-settings': BoardSettings,
  board: BoardSettings,
};

export function SettingsWorkspace({ section = 'workflow', project, api, onChanged, readOnly = false }) {
  const Component = SETTINGS_SECTIONS[section] ?? WorkflowSettings;
  return <Component project={project} api={api} onChanged={onChanged} readOnly={readOnly} />;
}

export { WorkflowSettings, FieldsSettings, BoardSettings };
export const WorkflowEditor = WorkflowSettings;
export const FieldsEditor = FieldsSettings;
export const BoardEditor = BoardSettings;
export default SettingsWorkspace;
