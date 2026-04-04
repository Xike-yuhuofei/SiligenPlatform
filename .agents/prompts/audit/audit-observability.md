# {{PROMPT_NAME}}

You are auditing one module in the SiligenSuite workspace.

Target module:
- module_name: {{MODULE_NAME}}
- module_path: {{MODULE_PATH}}
- repo_root: {{REPO_ROOT}}
- run_id: {{RUN_ID}}
- commit_sha: {{COMMIT_SHA}}

Assigned dimension:
- skill_name: {{SKILL_NAME}}
- dimension: {{AUDIT_DIMENSION}}
- dimension_label: {{AUDIT_DIMENSION_LABEL}}

Audit rules:
- Analyze only the assigned audit dimension.
- Use repository evidence from the target module and only the minimum adjacent files needed to explain boundary facts.
- Prefer module README, module.yaml, CMakeLists, contracts, tests, and direct consumers.
- Do not perform remediation planning.
- Do not modify code.
- Do not merge findings from other dimensions into this report.
- Keep the result machine-oriented and stable.

Required focus for this prompt:
- Verify whether the module exposes diagnostics, trace points, error signals, and evidence that explain behavior.
- Check whether runtime state is observable enough to support debugging and audits.
- Check whether failures are diagnosable at the module boundary.

Output contract:
- Produce exactly one final artifact as JSON.
- The JSON MUST conform to `.agents/schemas/report.schema.json`.
- Required fixed fields:
  - `skill_name` = `{{SKILL_NAME}}`
  - `dimension` = `{{AUDIT_DIMENSION}}`
  - `module_name` = `{{MODULE_NAME}}`
  - `run_id` = `{{RUN_ID}}`
  - `commit_sha` = `{{COMMIT_SHA}}`
- `findings` must include only observability and diagnostics issues.
- `evidence` must contain concrete repository paths and locators.
- Do not emit Markdown.
- Do not wrap JSON in code fences.
