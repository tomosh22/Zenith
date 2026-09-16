/**
 * Diagnostics are deliberately plain objects.  They are consumed by the CLI,
 * the app and tests, so keeping them serialisable also makes a failed cold
 * scan easy to report.
 */

export function diagnostic(code, message, details = {}) {
  const result = {
    code,
    message,
    severity: details.severity ?? 'error',
    ...details,
  };

  // Do not allow callers to accidentally replace the stable code/message
  // fields through the spread above.
  result.code = code;
  result.message = message;
  return result;
}

export function isErrorDiagnostic(value) {
  return value?.severity !== 'warning';
}

export function withPath(value, filePath, extra = {}) {
  return {
    ...value,
    path: value.path ?? filePath,
    ...extra,
  };
}

export function dedupeDiagnostics(values) {
  const seen = new Set();
  const result = [];
  for (const value of values ?? []) {
    if (!value) continue;
    const key = JSON.stringify([
      value.code,
      value.path,
      value.line,
      value.column,
      value.message,
      value.reference,
      value.target,
    ]);
    if (seen.has(key)) continue;
    seen.add(key);
    result.push(value);
  }
  return result;
}
