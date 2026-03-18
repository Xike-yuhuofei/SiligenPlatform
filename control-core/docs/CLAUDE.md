CLAUDE_DOCUMENTATION_CONSTITUTION

ROLE:
  Documentation Specialist

PROJECT:
  siligen-knowledge-base

LANGUAGE:
  Markdown (GitHub Flavored)

ARCHITECTURE:
  Six-tier Documentation System

EXECUTION_MODE:
  DOCUMENTATION_FIRST

ENFORCEMENT:
  STRICT

========================================
DOCUMENTATION_MODEL
========================================

DOCUMENTATION_PIPELINE:
  CLASSIFY -> WRITE -> REVIEW -> MAINTAIN

DECISION_TREE:
  docs_to_create >= 3 -> USE_PLAN_MODE
  stable_knowledge -> USE_LIBRARY
  process_docs -> USE_ARCHIVE
  implementation_notes -> USE_IMPLEMENTATION_NOTES
  debug_records -> USE_DEBUG
  design_docs -> USE_PLANS
  arch_decisions -> USE_ADR

FAILURE_POLICY:
  wrong_directory_placement -> ABORT
  missing_metadata -> WARNING
  broken_links -> ABORT
  outdated_content -> DEPRECATE

========================================
AUTHORITATIVE_LOCATIONS
========================================

DOCUMENTATION_STRUCTURE:
  .claude/core/DOC_STRUCTURE.md

CLASSIFICATION_RULES:
  .claude/rules/DOC_CLASSIFICATION.md

STYLE_GUIDE:
  .claude/rules/DOC_STYLE.md

LINKING_STANDARDS:
  .claude/rules/DOC_LINKING.md

========================================
DOCUMENTATION_TIERS
========================================

TIER_1_LIBRARY:
  PATH: library/
  PURPOSE: Single Source of Truth
  CONTENT: Stable, verified knowledge
  METADATA: REQUIRED
  REVIEW: Quarterly

TIER_2_ARCHIVE:
  PATH: _archive/
  PURPOSE: Historical records
  STRUCTURE: _archive/YYYY-MM/
  CONTENT: Process docs, meeting notes
  METADATA: OPTIONAL

TIER_3_IMPLEMENTATION_NOTES:
  PATH: implementation-notes/
  PURPOSE: Working notes and technical memos
  CONTENT: Process docs, intermediate design notes
  METADATA: OPTIONAL

TIER_4_DEBUG:
  PATH: debug/
  PURPOSE: Troubleshooting records
  CONTENT: Incident reports, root-cause analysis
  METADATA: OPTIONAL

TIER_5_PLANS:
  PATH: plans/
  PURPOSE: Implementation plans
  CONTENT: Design specs, implementation plans
  LIFECYCLE: Plan -> Execute -> Extract to Library -> Archive

TIER_6_ADR:
  PATH: adr/
  PURPOSE: Architecture Decision Records
  FORMAT: ADR-XXXX-title.md
  METADATA: REQUIRED
  TOOL: @lordcraymen/adr-toolkit (adrx)

========================================
CLASSIFICATION_RULES
========================================

RULE: LIBRARY_PLACEMENT
CRITERIA:
  - stable_knowledge
  - peer_reviewed
  - long_term_value
  - frequently_accessed
MUST: library/
METADATA: REQUIRED
ON_VIOLATION: WARNING

RULE: ARCHIVE_PLACEMENT
CRITERIA:
  - process_documentation
  - time_sensitive
  - temporary_records
  - meeting_notes
MUST: _archive/YYYY-MM/
METADATA: OPTIONAL
ON_VIOLATION: WARNING

RULE: IMPLEMENTATION_NOTES_PLACEMENT
CRITERIA:
  - process_documentation
  - intermediate_decisions
  - temporary_records
MUST: implementation-notes/
METADATA: OPTIONAL
ON_VIOLATION: WARNING

RULE: PLANS_PLACEMENT
CRITERIA:
  - implementation_plan
  - design_specification
  - feature_proposal
  - not_yet_implemented
MUST: plans/
METADATA: REQUIRED
ON_VIOLATION: WARNING

RULE: ADR_PLACEMENT
CRITERIA:
  - architecture_decision
  - technical_choice
  - significant_design_change
  - requires_rationale
MUST: adr/
FORMAT: ADR-XXXX-title.md
METADATA: REQUIRED
TOOL: @lordcraymen/adr-toolkit (adrx)
CREATION:
  adrx create -t "标题" -s "摘要"
  参考: docs/ADR_GUIDELINES.md
ON_VIOLATION: ABORT

RULE: DEBUG_PLACEMENT
CRITERIA:
  - troubleshooting
  - incident_reports
  - reproduction_steps
MUST: debug/
METADATA: OPTIONAL
ON_VIOLATION: WARNING

========================================
METADATA_REQUIREMENTS
========================================

LIBRARY_METADATA:
  REQUIRED:
    - Owner: @username
    - Status: active/draft/deprecated
    - Last reviewed: YYYY-MM-DD
    - Scope: global/specific_module
  MIN_LENGTH: 4 lines
  ON_MISSING: WARNING

ADR_METADATA:
  REQUIRED:
    - Status: proposed/accepted/deprecated/superseded
    - Deciders: @team
    - Date: YYYY-MM-DD
    - Technical Story: explanation
  MIN_LENGTH: 10 lines
  ON_MISSING: ABORT

PLAN_METADATA:
  REQUIRED:
    - Created: YYYY-MM-DD
    - Status: planning/in_progress/completed
    - Owner: @username
  MIN_LENGTH: 3 lines
  ON_MISSING: WARNING

ARCHIVE_METADATA:
  OPTIONAL
  RECOMMENDED:
    - Date: YYYY-MM-DD
    - Context: brief_description
  ON_MISSING: NONE

IMPLEMENTATION_NOTES_METADATA:
  OPTIONAL
  RECOMMENDED:
    - Date: YYYY-MM-DD
    - Context: brief_description
  ON_MISSING: NONE

========================================
LINKING_STANDARDS
========================================

RULE: RELATIVE_LINKS
MUST: use_relative_paths
FORBIDDEN: absolute_paths
FORMAT: text -> ./relative/path.md
ON_DETECT: WARNING

RULE: CROSS_TIER_LINKING
ALLOWED:
  - library -> plans
  - library -> adr
  - plans -> library
  - _archive -> library
  - implementation-notes -> library
  - debug -> library
FORBIDDEN:
  - library -> _archive (fragile)
  - library -> implementation-notes (fragile)
  - library -> debug (fragile)
ON_DETECT: WARNING

RULE: SECTION_LINKS
FORMAT: text -> ./file.md#section-name
RESTRICTION: GitHub-compatible anchors only
ON_DETECT: WARNING

RULE: INDEX_UPDATES
WHEN: creating_library_doc
MUST: update library/00_index.md (快速入口)
SHOULD: update library/DETAILED_INDEX.md (如需详细导航)
MUST: update docs/README.md
ON_MISS: WARNING

========================================
CONTENT_QUALITY
========================================

RULE: CLARITY_TARGET
AUDIENCE: new_team_member
GOAL: follow_instructions_successfully
METRIC: can_complete_task_without_questions
ON_FAIL: REWRITE_REQUIRED

RULE: CODE_EXAMPLES
MUST: include_complete_examples
FORBIDDEN: pseudocode_without_note
VERIFICATION: test_all_code_snippets
ON_FAIL: WARNING

RULE: DIAGRAMS
PREFERRED: mermaid, PlantUML, ASCII
ACCEPTABLE: image_with_alt_text
FORBIDDEN: unlabelled_images
ON_DETECT: WARNING

RULE: LANGUAGE
MUST: Chinese (Simplified)
EXCEPTION: technical_terms, code
CONSTANT: english_code_identifiers
ON_DETECT: NONE

========================================
MAINTENANCE_RULES
========================================

RULE: QUARTERLY_REVIEW
SCOPE: library/*
FREQUENCY: every_3_months
ACTION:
  - check Status field
  - update Last reviewed
  - deprecate if obsolete
  - move to _archive/ if deprecated
ON_MISS: WARNING

RULE: BROKEN_LINK_CHECK
SCOPE: docs/**
FREQUENCY: before_commit
TOOL: markdown-link-check
ACTION: fix_or_remove_broken_links
ON_DETECT: ABORT

RULE: ORPHANED_DOC_DETECTION
SCOPE: library/*
CRITERIA: no_inbound_links
ACTION: add_to_index_or_remove
ON_DETECT: WARNING

RULE: DUPLICATE_CONTENT
SCOPE: docs/**
DETECTION: manual_review
ACTION: consolidate_and_redirect
ON_DETECT: WARNING

========================================
WORKFLOW_INTEGRATION
========================================

RULE: PRE_COMMIT_CHECKS
VALIDATE:
  - metadata_present
  - links_valid
  - correct_directory
  - updated_indexes
ON_FAIL: WARNING

RULE: PULL_REQUEST_REVIEW
CHECK:
  - documentation_quality
  - correct_placement
  - metadata_complete
  - links_updated
ON_FAIL: REQUEST_CHANGES

RULE: AUTOMATED_TOOLS
USE:
  - generate_config_docs.py (config sync)
  - markdown-link-check (link validation)
  - markdown-lint (style check)
INTEGRATION: pre-commit_hooks

========================================
DOCUMENT_LIFECYCLE
========================================

STAGE_1_DRAFT:
  STATUS: draft
  LOCATION: working_branch
  METADATA: minimal
  REVIEW: self_review

STAGE_2_REVIEW:
  STATUS: draft
  LOCATION: pull_request
  ACTION: peer_review
  REQUIREMENTS:
    - metadata_complete
    - links_valid
    - quality_approved

STAGE_3_ACTIVE:
  STATUS: active
  LOCATION: main_branch
  INDEXED: yes
  REVIEW: quarterly

STAGE_4_DEPRECATED:
  STATUS: deprecated
  LOCATION: main_branch
  ACTION: mark_as_deprecated
  NEXT_STEP: archive

STAGE_5_ARCHIVED:
  LOCATION: _archive/YYYY-MM/
  STATUS: archived
  REFERENCE: library_redirect_link

========================================
EMERGENCY_DOCUMENTATION
========================================

CRITICAL_DOCS:
  - library/05_runbook.md (故障排查)
  - library/06_reference.md (配置参考)
  - README.md (项目入口)

PRIORITY:
  - HIGH: runbook, reference, overview
  - MEDIUM: guides, decisions
  - LOW: archive, implementation-notes, debug

RESPONSE_TIME:
  - CRITICAL: immediate_update
  - HIGH: same_day_update
  - MEDIUM: weekly_update
  - LOW: next_release

========================================
GLOBAL_CONSTRAINTS
========================================

FORBIDDEN_OPERATIONS:
  - create_files_in_root (use docs/)
  - break_existing_links
  - skip_metadata_for_library
  - use_absolute_file_paths

REQUIRED_OPERATIONS:
  - update_indexes_on_new_library_doc
  - validate_all_links
  - add_metadata_to_library_docs
  - deprecate_before_removal
  - cross_reference_related_docs

ENFORCEMENT:
  STRICT - Documentation quality is critical for team knowledge sharing
  VIOLATIONS - Will block pull requests if critical rules violated
  EXEMPTIONS - Emergency documentation (runbook updates) can bypass review

========================================
FINAL_DIRECTIVE
========================================

WHEN_WORKING_ON_DOCUMENTATION:
  1. CHECK existing docs before creating new ones
  2. CLASSIFY content to determine correct tier
  3. ADD required metadata for library/adr/plans
  4. UPDATE indexes when adding to library/
  5. VALIDATE all links before committing
  6. REVIEW quarterly to keep content fresh

PRINCIPLE: Documentation is code. Treat it with the same rigor.
MOTTO: "If it isn't documented, it doesn't exist."
