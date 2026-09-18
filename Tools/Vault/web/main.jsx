import React from 'react';
import { createRoot } from 'react-dom/client';
import { WorkspaceApp } from './WorkspaceApp.jsx';
import { WorkspaceBoundary } from './WorkspaceBoundary.jsx';
import './styles.css';

createRoot(document.getElementById('root')).render(
  <React.StrictMode><WorkspaceBoundary><WorkspaceApp /></WorkspaceBoundary></React.StrictMode>,
);
