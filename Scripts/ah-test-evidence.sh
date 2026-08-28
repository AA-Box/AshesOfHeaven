#!/usr/bin/env bash
# Shared definition of "which bytes can change an automation-test outcome", used by both the
# writer (Run-AutomationTests.sh) and the verifier (Validate-CrossPlatform.sh).
#
# Why this exists: the gameplay automation suite is EditorContext|CommandletContext, so it only
# runs inside an Unreal editor/commandlet. This repository is public, and a self-hosted runner
# on a public repository executes fork-authored code on the machine it is registered to, so
# there is no runner GitHub can schedule. Rather than let "automated tests pass before merge"
# go back to being an unverified claim, the result is recorded against a hash of the inputs
# that produced it and re-checked on every pull request by a hosted runner.
#
# This is an honesty ratchet, not a security boundary: someone who wants to commit a false
# result can. What it does prevent is the common failure - editing gameplay code and forgetting
# to re-run the suite - because the recorded hash no longer matches the tree.
#
# Docs/ is deliberately excluded, so a documentation-only change does not force a re-run. The
# evidence file itself lives in Docs/, which is also what keeps it out of its own hash.

# Two separate claims, two separate files. The automation evidence comes from
# Run-AutomationTests.sh, which builds ONLY `AshesOfHeavenEditor Mac Development` - it never
# performs a Shipping package, so it cannot speak for the macos-shipping job. Treating one as
# cover for the other would let a change that breaks Mac Shipping alone merge on a green gate.
AH_EVIDENCE_FILE="Docs/automation-evidence.json"
AH_SHIPPING_EVIDENCE_FILE="Docs/shipping-evidence.json"

ah_sha256() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum | cut -d' ' -f1
  else
    shasum -a 256 | cut -d' ' -f1
  fi
}

# Hash of the WORKING TREE contents, not the git index: a dirty tree must not be able to record
# evidence that describes the committed state instead of what was actually tested.
#
# --others --exclude-standard is what makes the hash independent of STAGING state. With
# --cached alone the file set is "what git tracks", so `git add` of a new source file changes
# the hash after the fact and silently invalidates evidence recorded minutes earlier. That is
# not hypothetical: it is how this function was first written, and CI caught it. A clean CI
# checkout has no untracked files, so --others is empty there and the two agree.
ah_inputs_hash() {
  git ls-files -z --cached --others --exclude-standard -- Source Config Content Scripts '*.uproject' \
    | xargs -0 shasum -a 256 \
    | LC_ALL=C sort \
    | ah_sha256
}
