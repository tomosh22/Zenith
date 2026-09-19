// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
class ConfigError extends Error {
  constructor(message) {
    super(message);
    this.name = "ConfigError";
  }
}
function projectConfig(config, key) {
  const p = config.projects[key];
  if (!p) throw new ConfigError("Project " + key + " is not mapped in agent.config.json. Known: " + (Object.keys(config.projects).join(", ") || "(none)"));
  return p;
}
export {
  ConfigError,
  projectConfig
};
