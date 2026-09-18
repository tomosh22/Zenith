import React from 'react';

export class WorkspaceBoundary extends React.Component {
  state = { failed: false };
  static getDerivedStateFromError() { return { failed: true }; }
  render() {
    if (this.state.failed) return <main className="project-error" role="alert">
      <h1>This view could not be displayed</h1>
      <p>Reload the workspace to recover. Unsaved edits may need to be entered again.</p>
      <button onClick={() => window.location.reload()}>Reload workspace</button>
    </main>;
    return this.props.children;
  }
}
