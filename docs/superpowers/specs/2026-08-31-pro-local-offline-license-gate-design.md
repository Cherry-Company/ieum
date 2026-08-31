# Pro Local Offline License Gate Design

## Status

This design replaces the unrestricted alpha.20 file-transfer preview with a
license-gated alpha.21 release. It also defines a private, local-only license
issuer for the project owner. The issuer, signing key, issuance ledger, and
owner acceptance license never enter Git or a GitHub artifact.

## Product decision

Starting with `0.1.0-alpha.21`, both sending and receiving files require a
valid Pro Local entitlement. Community KVM, IME synchronization, clipboard,
and the rest of the local core remain available without a license.

Alpha.20 stays online only until alpha.21 has been published and its
post-publication verification has succeeded. The cutover then deletes the
alpha.20 GitHub Release and its remote tag. Keeping alpha.20 during the build
preserves a recoverable download and lets the Windows migration job exercise
the real alpha.20-to-alpha.21 upgrade path.

## Scope

Alpha.21 includes:

- a versioned Ed25519 license envelope and strict verifier;
- one embedded production verification key with an explicit key ID;
- license import, removal, and status controls on the Windows Files settings
  page;
- GUI, application-start, and Windows transfer-service authorization checks;
- migration that clears alpha.20 send and receive preferences when no valid
  entitlement exists;
- a private Windows key generator and signing authority outside the repository;
- a private owner license for installing alpha.21 on the owner's own machines;
- supporter instructions, public product documentation, translations, tests,
  the generated code graph, CI, and release evidence.

This release does not add payment processing, accounts, an activation server,
device counting, remote revocation, or a proprietary desktop component.

## License envelope

A license is one printable line:

```text
IEUM1.<base64url payload>.<base64url Ed25519 signature>
```

The signature covers the payload bytes exactly as encoded. The payload is
compact UTF-8 JSON with these fields:

```json
{
  "schema": "ieum.pro-license.v1",
  "key_id": "pro-local-2026-01",
  "license_id": "lowercase UUID",
  "product": "pro-local",
  "features": ["file-transfer"],
  "recipient": "supporter alias or order reference",
  "issued_at": "UTC ISO-8601 timestamp",
  "not_before": "UTC ISO-8601 timestamp",
  "expires_at": null
}
```

The private issuer sorts JSON keys and uses compact separators so issued files
are reproducible. The application verifies the received bytes instead of
re-serializing JSON. It rejects:

- a missing or incorrect prefix;
- anything other than three dot-separated sections;
- invalid or padded base64url;
- a payload larger than 2 KiB or a complete license larger than 4 KiB;
- a signature whose decoded size is not 64 bytes;
- malformed JSON, a non-object root, or a wrong schema;
- an unknown key ID, malformed UUID, wrong product, or missing
  `file-transfer` feature;
- invalid UTC timestamps, a future `not_before`, or an elapsed `expires_at`;
- any signature that does not verify against the selected 32-byte Ed25519
  public key.

Supporter licenses are perpetual by default (`expires_at` is `null`). The
private issuer can create a dated license when needed. Recipient text is a
display label, not an email address or other required personal identifier.

## Public verifier

A focused `licensing` library owns parsing, signature verification, status
messages, and feature decisions. It depends on Qt Core and OpenSSL Crypto, not
on GUI or file-transfer code. Its public interface accepts an explicit time and
key ring for deterministic tests. Production helpers use the embedded key ring
and current UTC time.

The verifier returns a structured result rather than a boolean. Status values
distinguish missing, malformed, unsupported, bad-signature, not-yet-valid,
expired, and valid licenses. Logs may record only the status and license ID;
they never record the complete license text, signature, or recipient.

The public key is not secret. Publishing it is required for local verification
and does not allow license creation. Only the corresponding Ed25519 private key
can create a signature accepted by the official binary.

## Files settings flow

The Windows Files tab begins with a Pro Local license group:

- a masked single-line license field;
- `Import license file`, `Activate`, and `Remove` buttons;
- a status label showing locked, invalid, expired, or active state;
- the active license ID and recipient label, but never the full key.

Import accepts a UTF-8 text file containing one license line, trims surrounding
whitespace, enforces the 4 KiB input limit, and places it in the masked field.
Activate validates the field and saves it only when valid. Remove clears the
saved license and immediately disables both transfer preferences. The sender,
receiver, and destination controls are enabled only when all of these are true:

- settings are writable;
- the platform is Windows;
- TLS is selected;
- the stored Pro license currently permits `file-transfer`.

An invalid key never replaces a previously valid stored key. Settings and logs
must not reveal the full license.

## Runtime enforcement

The GUI is not the security boundary. Enforcement happens at three layers:

1. Settings never enable the file controls without a valid entitlement.
2. `ClientApp` and `ServerApp` refuse to start a file-transfer service without
   a current entitlement, even when configuration flags were edited manually.
3. `MSWindowsFileTransferService` receives an authorization callback and checks
   it before every new offer, accepted incoming offer, and outgoing data step.

When alpha.21 first sees alpha.20 transfer preferences without a valid license,
it clears both preferences. Adding a key later does not silently resume file
transfer; the user must explicitly enable sending or receiving again.

Both computers need a valid key. A key may be copied to the owner's or
supporter's own machines because this offline release does not enforce a seat
count. Online activation and device limits belong to the later account and
entitlement service.

## Private issuer kit

The owner receives a local-only directory outside every Git worktree:

```text
C:\Users\LYR\Ieum-License-Authority-PRIVATE\
  IeumLicenseKeygen.exe
  Ieum-Pro-Local-Ed25519-private.pem
  Ieum-Pro-Local-public-key.txt
  README-PRIVATE.txt
  ledger.jsonl
  issued\
    ieum-owner-pro-local.ieum-license.txt
    ieum-owner-pro-local.receipt.json
```

NTFS permissions grant access only to the current user, Administrators, and
SYSTEM. The directory is never copied into the repository. The owner must keep
an encrypted offline backup of the private PEM; losing it prevents new keys
from being issued for builds that trust this public key.

The key generator is a standalone Windows GUI. It asks for a supporter alias
or donation/order reference, offers an optional expiration date, and writes:

- `<license-id>.ieum-license.txt`, the single file sent to the supporter;
- `<license-id>.receipt.json`, a private issuance receipt;
- one append-only `ledger.jsonl` record containing the license ID, display
  recipient, timestamps, output filename, and SHA-256 of the complete license.

The generator does not send email, contact a server, print the private key, or
place the full license in the ledger. It refuses to overwrite an existing
license or receipt. The private source used to build the executable also stays
in this directory and is not committed.

The supporter receives only the `.ieum-license.txt` file. They open Files
settings on both Windows computers, import the file, activate it, enable TLS,
and then opt into sending or receiving.

## Testing

The committed test suite uses a test public key and pre-signed fixture. It does
not contain the production private key or issuer source. Tests cover:

- valid perpetual and dated licenses;
- tampered payloads and signatures;
- wrong prefix, segment count, base64url, size, schema, key ID, product,
  feature, UUID, and timestamps;
- future and expired licenses at fixed UTC instants;
- runtime access with absent, invalid, and valid settings;
- service rejection after authorization is withdrawn;
- alpha.20 preference migration;
- the settings import limit and status mapping;
- a private end-to-end check in which the local key generator creates a key
  accepted by the production verifier.

Focused tests run before the protected repository CI matrix. The release keeps
the existing Windows x64 and ARM64 package migration tests, exact release byte
scan, checksums, and post-fetch verification.

## Release sequence

1. Implement on `feat/pro-license-gate-alpha21` using test-first changes.
2. Build the private authority and key generator outside Git; create the owner
   acceptance license and verify it against the production public key.
3. Set `VERSION` to `0.1.0-alpha.21`, update public documentation and
   translations, and regenerate the code graph.
4. Push the feature branch, pass protected PR CI, and merge into `ieum/main`.
5. Create and push the annotated `v0.1.0-alpha.21` tag.
6. Wait for package, release privacy, deployment, and post-fetch jobs to pass.
7. Download the public assets and independently verify every checksum.
8. Delete the alpha.20 GitHub Release and remote tag, then verify both are gone.

Alpha.20 is not deleted before step 6 succeeds. The private issuer directory is
checked for absence from `git status`, `git ls-files`, commits, release assets,
and privacy evidence before publication.

## Commercial and GPL boundary

The official alpha.21 binary uses the license gate, but the GPL-covered source
remains modifiable and redistributable. The offline gate identifies supported
official use; it is not described as unbreakable DRM or hidden source. Hosted
activation, discovery, and relay can later provide stronger server-side
entitlement enforcement without moving proprietary code into the GPL desktop
executable.
