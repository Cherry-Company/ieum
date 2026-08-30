# Public privacy guard

Ieum scans public Git trees, incoming commit ranges, release bytes, generated
source archives, and post-publication bytes with
`tools/privacy/public_privacy_guard.py`. The public detector contains no private
identity values. It requires an owner-private deny map and that map's separately
frozen SHA-256 digest.

## Private inputs

Set both variables before using the local hook:

```sh
export IEUM_PRIVATE_DENY_MAP=/owner-private/path/ieum-private-deny-map.json
export IEUM_PRIVATE_DENY_MAP_SHA256=<frozen-lowercase-sha256>
export IEUM_PRIVACY_BASELINE=<reviewed-clean-commit>
```

The deny-map file must be strict UTF-8 JSON, must use schema
`ieum.private-deny-map.v1`, and must not grant group or other filesystem access.
The scanner records only rule IDs, target identities, byte offsets, and input
hashes; it does not copy private values into a public manifest.

## Install and verify the pre-push hook

After the guard commit is reviewed locally, install the tracked hook and verify
the active repository-local setting:

```sh
tools/privacy/install_hook.sh --install
tools/privacy/install_hook.sh --check
git config --local --get core.hooksPath
```

The final command must print exactly `.githooks`. The hook runs both `tree` and
`range` for create, update, and force-update ref lines. A deletion introduces no
new public bytes and is reported without inferring mutation authority. A new
ref requires `IEUM_PRIVACY_BASELINE`; missing or changed private inputs fail
closed. Git's `--no-verify` can bypass a local hook, so protected-branch and tag
rules remain a separate required control.

## CI and release boundaries

The CI workflow materializes the deny map from a private secret into a temporary
mode-0600 file and runs synthetic tests plus the tree/range gates. The release
job scans the exact final body, generated notes, checksum manifest, every
package, and both provider-generated source archives before it creates a Release
object. ZIP and tar-family archives use the standard-library decoder. Release
packages additionally use bounded `zstd` and 7-Zip processes; Flatpak bundles
are imported into a temporary OSTree repository and exported as a tar stream.
Those helpers run with an isolated home, a 120-second deadline, the configured
per-file limit, and no inherited Git controls. Flatpak, DMG, and
provider-generated source archive links are accepted only when they resolve
inside the extracted package or archive; their link text is scanned as release
data. Unsupported, encrypted, unsafe or unresolved linked,
unreadable, traversing, nested beyond the declared limit, or oversized archive
material is `unverified` and blocks publication.

The post-fetch job compares the complete advertised ref map with the frozen
pre-release map, scans the exact remote tip/range and downloaded public bytes,
then compares the ref map again. Any drift, missing input, private-map mismatch,
finding, or incomplete expansion fails closed. These controls prevent new
publication mistakes; they do not declare existing tags or reachable history
clean and do not authorize any GitHub or ref mutation.
