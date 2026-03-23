#!/usr/bin/env bash

set -e

show_usage() {
    cat <<'EOF'
Usage: ./create-new-feature.sh [--json] [--type <type>] --scope <scope> [--ticket <ticket>] [--short-name <name>] <feature_description>

Options:
  --json              Output in JSON format
  --type <type>       Branch type: feat|fix|chore|refactor|docs|test|hotfix|spike|release (default: feat)
  --scope <scope>     Module or domain short name, e.g. hmi, runtime, cli
  --ticket <ticket>   Task ID like SS-142, or NOISSUE when none exists yet (default: NOISSUE)
  --short-name <name> Provide a custom short description for the branch tail
  --help, -h          Show this help message

Examples:
  ./create-new-feature.sh --type chore --scope hmi --ticket SS-142 --short-name debug-instrumentation "Add HMI debug instrumentation"
  ./create-new-feature.sh --scope runtime "Tighten startup timeout handling"
EOF
}

JSON_MODE=false
SHORT_NAME=""
TYPE="feat"
SCOPE=""
TICKET="NOISSUE"
LEGACY_NUMBER=""
USE_TIMESTAMP=false
ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --json)
            JSON_MODE=true
            shift
            ;;
        --short-name)
            [[ $# -ge 2 ]] || { echo 'Error: --short-name requires a value' >&2; exit 1; }
            SHORT_NAME="$2"
            shift 2
            ;;
        --type)
            [[ $# -ge 2 ]] || { echo 'Error: --type requires a value' >&2; exit 1; }
            TYPE="$2"
            shift 2
            ;;
        --scope)
            [[ $# -ge 2 ]] || { echo 'Error: --scope requires a value' >&2; exit 1; }
            SCOPE="$2"
            shift 2
            ;;
        --ticket)
            [[ $# -ge 2 ]] || { echo 'Error: --ticket requires a value' >&2; exit 1; }
            TICKET="$2"
            shift 2
            ;;
        --number)
            [[ $# -ge 2 ]] || { echo 'Error: --number requires a value' >&2; exit 1; }
            LEGACY_NUMBER="$2"
            shift 2
            ;;
        --timestamp)
            USE_TIMESTAMP=true
            shift
            ;;
        --help|-h)
            show_usage
            exit 0
            ;;
        --)
            shift
            ARGS+=("$@")
            break
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

FEATURE_DESCRIPTION="${ARGS[*]}"
FEATURE_DESCRIPTION=$(echo "$FEATURE_DESCRIPTION" | xargs)
if [[ -z "$FEATURE_DESCRIPTION" ]]; then
    show_usage >&2
    exit 1
fi

if [[ -n "$LEGACY_NUMBER" ]]; then
    echo "Error: --number is no longer supported. Use --type, --scope, and --ticket to create a compliant branch." >&2
    exit 1
fi

if [[ "$USE_TIMESTAMP" == true ]]; then
    echo "Error: --timestamp is no longer supported. Use --type, --scope, and --ticket to create a compliant branch." >&2
    exit 1
fi

if [[ -z "$SCOPE" ]]; then
    echo "Error: --scope is required. Use a short module/domain name such as hmi, runtime, cli, or gateway." >&2
    exit 1
fi

find_repo_root() {
    local dir="$1"
    while [[ "$dir" != "/" ]]; do
        if [[ -d "$dir/.git" ]] || [[ -d "$dir/.specify" ]]; then
            echo "$dir"
            return 0
        fi
        dir="$(dirname "$dir")"
    done
    return 1
}

clean_branch_name() {
    local name="$1"
    echo "$name" | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9]/-/g' | sed 's/-\+/-/g' | sed 's/^-//' | sed 's/-$//'
}

normalize_scope() {
    local value
    value=$(clean_branch_name "$1")
    [[ -n "$value" ]] || { echo "Scope must contain at least one alphanumeric character." >&2; exit 1; }
    echo "$value"
}

normalize_ticket() {
    local value
    value=$(echo "$1" | tr '[:lower:]' '[:upper:]' | tr -d '[:space:]')
    if [[ ! "$value" =~ ^(NOISSUE|[A-Z][A-Z0-9]*-[0-9]+)$ ]]; then
        echo "Ticket must be NOISSUE or match PROJECT-123 style identifiers." >&2
        exit 1
    fi
    echo "$value"
}

generate_branch_name() {
    local description="$1"
    local stop_words='^(i|a|an|the|to|for|of|in|on|at|by|with|from|is|are|was|were|be|been|being|have|has|had|do|does|did|will|would|should|could|can|may|might|must|shall|this|that|these|those|my|your|our|their|want|need|add|get|set)$'
    local clean_name
    local meaningful_words=()

    clean_name=$(echo "$description" | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9]/ /g')

    for word in $clean_name; do
        [[ -n "$word" ]] || continue
        if ! echo "$word" | grep -qiE "$stop_words"; then
            if [[ ${#word} -ge 3 ]]; then
                meaningful_words+=("$word")
            elif echo "$description" | grep -q "\b${word^^}\b"; then
                meaningful_words+=("$word")
            fi
        fi
    done

    if [[ ${#meaningful_words[@]} -gt 0 ]]; then
        local max_words=3
        local result=""
        local count=0

        [[ ${#meaningful_words[@]} -eq 4 ]] && max_words=4

        for word in "${meaningful_words[@]}"; do
            [[ $count -lt $max_words ]] || break
            [[ -z "$result" ]] || result="$result-"
            result="$result$word"
            count=$((count + 1))
        done
        echo "$result"
        return
    fi

    local cleaned
    cleaned=$(clean_branch_name "$description")
    echo "$cleaned" | tr '-' '\n' | grep -v '^$' | head -3 | tr '\n' '-' | sed 's/-$//'
}

case "$TYPE" in
    feat|fix|chore|refactor|docs|test|hotfix|spike|release)
        ;;
    *)
        echo "Error: --type must be one of feat, fix, chore, refactor, docs, test, hotfix, spike, release." >&2
        exit 1
        ;;
esac

SCRIPT_DIR="$(CDPATH="" cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

if git rev-parse --show-toplevel >/dev/null 2>&1; then
    REPO_ROOT=$(git rev-parse --show-toplevel)
    HAS_GIT=true
else
    REPO_ROOT="$(find_repo_root "$SCRIPT_DIR")"
    [[ -n "$REPO_ROOT" ]] || { echo "Error: Could not determine repository root. Please run this script from within the repository." >&2; exit 1; }
    HAS_GIT=false
fi

cd "$REPO_ROOT"

SCOPE_VALUE=$(normalize_scope "$SCOPE")
TICKET_VALUE=$(normalize_ticket "$TICKET")

if [[ -n "$SHORT_NAME" ]]; then
    BRANCH_SUFFIX=$(clean_branch_name "$SHORT_NAME")
else
    BRANCH_SUFFIX=$(generate_branch_name "$FEATURE_DESCRIPTION")
fi

[[ -n "$BRANCH_SUFFIX" ]] || { echo "Error: Could not derive a short description. Provide --short-name explicitly." >&2; exit 1; }

BRANCH_NAME="$TYPE/$SCOPE_VALUE/$TICKET_VALUE-$BRANCH_SUFFIX"
PREFIX="$TYPE/$SCOPE_VALUE/$TICKET_VALUE-"
MAX_BRANCH_LENGTH=244

if [[ ${#BRANCH_NAME} -gt $MAX_BRANCH_LENGTH ]]; then
    MAX_SUFFIX_LENGTH=$((MAX_BRANCH_LENGTH - ${#PREFIX}))
    if [[ $MAX_SUFFIX_LENGTH -le 0 ]]; then
        echo "Error: Branch prefix '$PREFIX' is too long. Shorten --scope or --ticket." >&2
        exit 1
    fi

    TRUNCATED_SUFFIX=$(echo "$BRANCH_SUFFIX" | cut -c1-"$MAX_SUFFIX_LENGTH" | sed 's/-$//')
    ORIGINAL_BRANCH_NAME="$BRANCH_NAME"
    BRANCH_NAME="${PREFIX}${TRUNCATED_SUFFIX}"

    >&2 echo "[specify] Warning: Branch name exceeded GitHub's 244-byte limit"
    >&2 echo "[specify] Original: $ORIGINAL_BRANCH_NAME (${#ORIGINAL_BRANCH_NAME} bytes)"
    >&2 echo "[specify] Truncated to: $BRANCH_NAME (${#BRANCH_NAME} bytes)"
fi

SPECS_DIR="$REPO_ROOT/specs"
mkdir -p "$SPECS_DIR"
FEATURE_DIR="$SPECS_DIR/$BRANCH_NAME"

if [[ "$HAS_GIT" == true ]]; then
    if ! git checkout -b "$BRANCH_NAME" 2>/dev/null; then
        if git branch --list "$BRANCH_NAME" | grep -q .; then
            echo "Error: Branch '$BRANCH_NAME' already exists. Use a different --type/--scope/--ticket combination or --short-name." >&2
            exit 1
        fi

        echo "Error: Failed to create git branch '$BRANCH_NAME'. Please check your git configuration and try again." >&2
        exit 1
    fi
else
    echo "[specify] Warning: Git repository not detected; skipped branch creation for $BRANCH_NAME" >&2
fi

mkdir -p "$FEATURE_DIR"

TEMPLATE=$(resolve_template "spec-template" "$REPO_ROOT") || true
SPEC_FILE="$FEATURE_DIR/spec.md"
if [[ -n "$TEMPLATE" ]] && [[ -f "$TEMPLATE" ]]; then
    cp "$TEMPLATE" "$SPEC_FILE"
else
    echo "Warning: Spec template not found; created empty spec file" >&2
    touch "$SPEC_FILE"
fi

SPECIFY_FEATURE="$BRANCH_NAME"
export SPECIFY_FEATURE
SPEC_PATH="${FEATURE_DIR#$REPO_ROOT/}"

if $JSON_MODE; then
    if has_jq; then
        jq -cn \
            --arg branch_name "$BRANCH_NAME" \
            --arg spec_file "$SPEC_FILE" \
            --arg spec_path "$SPEC_PATH" \
            --arg feature_ref "$TICKET_VALUE" \
            --arg has_git "$HAS_GIT" \
            '{BRANCH_NAME:$branch_name,SPEC_FILE:$spec_file,SPEC_PATH:$spec_path,FEATURE_REF:$feature_ref,HAS_GIT:$has_git}'
    else
        printf '{"BRANCH_NAME":"%s","SPEC_FILE":"%s","SPEC_PATH":"%s","FEATURE_REF":"%s","HAS_GIT":"%s"}\n' \
            "$(json_escape "$BRANCH_NAME")" "$(json_escape "$SPEC_FILE")" "$(json_escape "$SPEC_PATH")" "$(json_escape "$TICKET_VALUE")" "$(json_escape "$HAS_GIT")"
    fi
else
    echo "BRANCH_NAME: $BRANCH_NAME"
    echo "SPEC_FILE: $SPEC_FILE"
    echo "SPEC_PATH: $SPEC_PATH"
    echo "FEATURE_REF: $TICKET_VALUE"
    echo "HAS_GIT: $HAS_GIT"
    printf '# To persist in your shell: export SPECIFY_FEATURE=%q\n' "$BRANCH_NAME"
fi
