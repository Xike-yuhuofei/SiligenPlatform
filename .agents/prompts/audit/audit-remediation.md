# audit-remediation

You are planning remediation work for one audited module in the SiligenSuite workspace.

Inputs:
- module_name: {{MODULE_NAME}}
- repo_root: {{REPO_ROOT}}
- run_id: {{RUN_ID}}
- commit_sha: {{COMMIT_SHA}}
- source_audit_index_path: {{AUDIT_INDEX_PATH}}

Planning rules:
- Read only `{{AUDIT_INDEX_PATH}}` as the audit source of truth.
- Do not reread individual `report.json` files unless the index says they are missing and planning must explain a blocker.
- Do not redo auditing.
- Do not modify code.
- Produce a machine-oriented remediation plan for follow-up implementation work.
- Prefer fewer workstreams with clearer ownership over broad reorganization ideas.

Required focus:
- Turn the highest-priority and highest-confidence findings into an execution order.
- Preserve module boundaries and avoid introducing new abstraction unless clearly necessary.
- Make blockers explicit when `planner_ready` is false or evidence is incomplete.
- Tie every workstream back to specific source findings from the audit index.

Output contract:
- Produce exactly one final artifact as JSON.
- The JSON MUST conform to `.agents/schemas/module-optimization-plan.schema.json`.
- Required fixed fields:
  - `module_name` = `{{MODULE_NAME}}`
  - `run_id` = `{{RUN_ID}}`
  - `commit_sha` = `{{COMMIT_SHA}}`
  - `source_audit_index_path` = `{{AUDIT_INDEX_PATH}}`
- `workstreams` must be prioritized and traceable to `source_findings`.
- `execution_order` must reference only `workstreams[].id`.
- Do not emit Markdown.
- Do not wrap JSON in code fences.
