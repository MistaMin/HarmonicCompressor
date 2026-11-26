#!/bin/sh
# Finds OpenPluginSDK and writes the untracked .cargo/config.toml that patches it in, so the
# repository holds no machine paths. Override the search with OPENPLUGIN_SDK=<path>.
#   . scripts/sdk_env.sh      (sets SDK_DIR; run before cargo or the SDK's build_plugin.sh)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
find_sdk() {
  if [ -n "$OPENPLUGIN_SDK" ] && [ -d "$OPENPLUGIN_SDK" ]; then (cd "$OPENPLUGIN_SDK" && pwd); return 0; fi
  for base in "$ROOT/.." "$ROOT/../.." "$ROOT/../Dependencies" "$ROOT/../../Dependencies" /Volumes/*/Dependencies "$HOME/Dependencies"; do
    [ -d "$base/OpenPluginSDK/crates/openplugin" ] && { (cd "$base/OpenPluginSDK" && pwd); return 0; }
  done
  return 1
}
SDK_DIR="$(find_sdk)" || { echo "OpenPluginSDK not found; set OPENPLUGIN_SDK" >&2; return 1 2>/dev/null || exit 1; }
export SDK_DIR
mkdir -p "$ROOT/.cargo"
cat > "$ROOT/.cargo/config.toml" <<EOF
# Written by scripts/sdk_env.sh; not tracked.
[patch.crates-io]
openplugin = { path = "$SDK_DIR/crates/openplugin" }
EOF
