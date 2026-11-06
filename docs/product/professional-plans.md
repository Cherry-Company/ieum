# Professional plans

Status: implementation in progress. This document describes the requested
product packaging, not a claim that the new features or checkout are live.
The published release remains alpha.31 until the release checks below pass.

## Product rules

| Plan | Includes |
| --- | --- |
| Free | Keyboard and mouse sharing, Korean/English input synchronization, text clipboard, encrypted local connections |
| Professional Local | Free plus direct file transfer |
| Professional | Local plus computer audio sharing |
| Professional Max | Professional plus control from either computer's keyboard/mouse, remote screen viewing, image clipboard, coordinated lock/wake, and simultaneous input |

Paid plans are cumulative. The ordinary purchase path is a subscription.
The separate event-license field accepts signed promotional and owner licenses.
Existing signed Pro Local licenses retain their original expiration, including
perpetual file-transfer access. A subscription ending must not erase a valid
legacy or event license or stop free input sharing.

Image clipboard and keyboard broadcast already exist in the inherited core.
Moving them into Max changes the entitlement policy; it is not new feature
development. That change must be disclosed in the release notes. Audio,
bidirectional control and remote viewing require native implementation and
cross-computer acceptance before they can be sold as available.

## Implementation status

The current branch contains the four-tier catalog, v2 signed entitlements,
separate event/subscription authorities, legacy-license fallback, periodic
subscription refresh, a translated plans dialog, and receipt-link handling.
The owner-only issuer and subscription API are separate private workspaces.
Native Linux builds, license boundaries and local API fixtures have been tested.

Audio capture/playback, remote screen transport and bidirectional input
ownership are not implemented in this branch. Coordinated lock/wake remains
incomplete. Existing image clipboard and keyboard broadcast have not yet been
restricted by the new Max entitlement. The catalog describes the intended
bundles; it is not runtime enforcement of these six features.

Checkout is unavailable in the desktop until a verified HTTPS service address
is set. Receipt links prefill the selected plan and a masked purchase key;
the user explicitly selects Restore before activation. Installer registration
and live receipt substitution still require Windows/macOS acceptance.

## Initial price hypothesis

All amounts are USD before any applicable checkout tax. Annual billing is shown
first, with the full annual charge visible; monthly billing is an alternative.

| Plan | Monthly | Annual | Annual saving against 12 monthly payments |
| --- | ---: | ---: | ---: |
| Free | $0 | $0 | — |
| Professional Local | $1.99 | $14.99 | 37.2% |
| Professional | $2.99 | $24.99 | 30.4% |
| Professional Max | $4.99 | $39.99 | 33.2% |

This is a launch hypothesis based on the public offers below, not measured
willingness to pay or a guaranteed conversion forecast. The distinguishing
value is a Windows/Mac workflow with Korean input support. Neither encryption
nor recovery from a failed payment is sold as a premium feature. Cancellation
must be accessible, with access continuing through the paid period.

## Market evidence, checked 2026-11-06

| Product | Observed offer | Relevance and limits |
| --- | --- | --- |
| [Synergy 3](https://symless.com/synergy/purchase) | Lite $15 / Plus $19 / Max $29, one-time; 3 / 5 / 15 computers | Low one-time price makes an expensive basic-KVM subscription unattractive. Audio is not included. |
| [Multiplicity 4 Pro](https://store.steampowered.com/app/3751880/Multiplicity_4_Pro/) | $49.99 on the publisher's Steam listing | Audio, files and remote screens; Windows only. Its one-time purchase is a meaningful alternative to annual Max. |
| [Multiplicity 4 launch announcement](https://www.stardock.com/news/532548/now-available-multiplicity-4-with-seamless-display-arm-support-and-more) | Base edition launched at $29.99 in December 2024 | Historical baseline, not independently confirmed current checkout pricing. |
| [ShareMouse](https://www.sharemouse.com/shop) | One-time license with 12 months of maintenance; Standard supports 2 computers and 4 displays | Windows/Mac and control from any keyboard. The extracted official shop did not expose a reliable numeric price; reseller estimates are excluded. |
| [Mouse Without Borders](https://learn.microsoft.com/en-us/windows/powertoys/mouse-without-borders) | Included in free PowerToys; up to 4 Windows computers | Free input, clipboard and simultaneous-input alternative; Windows only. |

The Max annual price is below the observed Multiplicity Pro one-time price in
year one, but costs more over multiple years. Marketing must not hide this
distinction or claim an objectively cheapest product.

## Simple checkout

Gumroad is the initial provider: hosted membership checkout, receipts, license
keys and customer subscription management. A buyer should be able to use the
activation link from their receipt; manual purchase-key entry is a fallback.
Event licenses use the private Ieum authority, independently of Gumroad.

[Direct sales cost 10% + $0.50](https://gumroad.com/pricing); discovery sales
have a different 30% fee. At the listed annual prices, the direct-sale amount
after that stated platform fee is approximately $12.99 / $21.99 / $35.49,
before any other applicable costs, taxes, refunds or payout charges. This is
why annual billing is the default rather than a very low monthly-only offer.
[Gumroad's payout guide](https://gumroad.com/help/article/13-getting-paid)
lists South Korean bank accounts. Seller verification still has to succeed.

[Lemon Squeezy](https://www.lemonsqueezy.com/pricing) is an alternative with a
5% + $0.50 base fee and built-in subscription license expiry. Its documented
[surcharges](https://docs.lemonsqueezy.com/help/getting-started/fees) include
subscriptions, international payments and some payout methods. South Korea is
absent from the checked bank-payout list; PayPal eligibility needs separate
confirmation. Lower base fees alone do not establish a simpler seller setup.

Gumroad's license API leaves keys enabled after subscription cancellation.
The integration must inspect the membership end timestamps, refunds, disputes,
the exact product ID and seller ID; `success: true` alone is insufficient.
The API's `increment_uses_count` must be false for periodic verification.
[Provider semantics](https://gumroad.com/help/article/76-license-keys).

## Before public sale

- Real approved seller account, product IDs, monthly/annual prices and checkout URLs.
- Private signing authority backed up outside Git and release artifacts.
- Correct permissions in GUI and core, on both peers where applicable.
- Renewal, scheduled cancellation, expiry, refund, wrong-product and offline tests.
- Windows/Mac physical acceptance of audio, capture permission, input ownership,
  key release, clipboard, lock/wake and simultaneous input.
- Translated in-app plans and README agree with actual released capabilities.
- Production checkout and activation tested with the configured provider.
- Native build/package checks and published installer checksum verification.
