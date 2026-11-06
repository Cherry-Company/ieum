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
incomplete. Image clipboard and keyboard broadcast now check the Max entitlement
in the core. Free clipboard sharing retains plain text, including text copied
alongside an image; HTML is excluded because it can embed images. Broadcast
repeats and key releases follow the original recipient set through a mode change,
license expiry or peer disconnect. This is keyboard broadcast, not completed
mouse broadcast or control from a secondary computer's physical keyboard.
The six-feature bundle is not ready for public sale.

Checkout is unavailable in the desktop until a verified HTTPS service address
is set through the release-time CMake option `IEUM_SUBSCRIPTION_SERVICE_URL`.
It accepts an HTTPS origin; neither a receipt nor user settings can override the
server receiving purchase keys. Hosting is independent of Vercel. Receipt links prefill the selected plan and a masked purchase key;
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

## Payment provider decision, checked 2026-11-06

The planning assumption is a Korean sole proprietor selling globally, possibly
incorporating later. Polar is the first integration candidate for a simpler
start; Paddle remains a credible alternative for broader checkout needs. Neither
approval nor a legal-entity migration has been verified for this seller. There
is no merchant account or live checkout yet.

| Provider | Published cost for a new seller | Assessment for Ieum |
| --- | --- | --- |
| [Polar](https://polar.sh/resources/pricing) | Starter: 5% + $0.50; +1.5% for non-US cards. No monthly fee. Payout and conversion costs are additional. | Hosted subscriptions and built-in license benefits; [South Korea is on the payout list](https://polar.sh/docs/merchant-of-record/supported-countries). First candidate for the user's preference for simpler setup. |
| [Paddle](https://www.paddle.com/pricing) | Published all-inclusive 5% + $0.50; contact sales for products under $10. | Established software subscription/MoR alternative. [Sellers worldwide except listed exclusions](https://www.paddle.com/help/start/account-verification/which-countries-are-supported-by-paddle); South Korea is not excluded. [Sole traders skip the separate business-identification step](https://www.paddle.com/help/start/account-verification/what-is-business-verification); domain/identity checks still apply. |
| [Lemon Squeezy](https://docs.lemonsqueezy.com/help/getting-started/fees) | 5% + $0.50; +0.5% subscriptions; +1.5% international; additional PayPal/payout charges can apply. | Korean payout eligibility needs confirmation for the actual seller and payout route. Base fee alone does not establish the easiest onboarding. |
| [Gumroad](https://gumroad.com/help/article/66-gumroads-fees) | Direct: 10% + $0.50, **plus card processing 2.9% + $0.30**. Discover: 30% including processing. | Simple storefront, but too expensive for Ieum's low monthly prices. No longer the recommended default. |

The earlier Gumroad comparison omitted the separately charged card-processing
fee. At $1.99, the combined direct card fee is approximately $1.06 (53.1%). At
$14.99 it is $2.73 (18.2%). This correction materially changes the recommendation.
Polar's 4% + $0.40 Early Member price does not apply to a new account: it is for
organizations created before May 27, 2026. The table uses new-seller pricing.

For a $14.99 annual sale, Polar Starter leaves approximately $13.74 for a US card
or $13.52 for a non-US card, before payout/conversion costs, tax effects, refunds
or disputes. At $1.99 monthly, even Polar's base fee consumes about 30.1%; annual
billing should remain the default, with the full charge shown. These arithmetic
examples are not a net-profit forecast.

The separate service has provider adapters for Polar and Gumroad, a stable Ieum
plan catalog, and its own subscription signing authority. The app uses generic
purchase/receipt wording and accepts only configured provider checkout hosts.
The private owner/event authority remains separate. A future legal-entity change
requires the provider's approval and payout-account review; changing business
registration alone does not migrate live subscriptions. Preserve the old payment
account and refresh path until a verified migration is complete. Do not rotate
Ieum license authorities merely because the business entity changes.

The Polar adapter pins API version 2026-10. It verifies the organization, exact
plan product and license benefit, the granted license key, active subscription,
current paid period and latest subscription order. It rejects trials, unpaid or
fully refunded orders, and caps a signed lease at the earliest applicable end,
with a maximum of seven days. Period-end cancellation retains paid access. A
[subscription refund alone does not revoke Polar benefits](https://polar.sh/docs/features/refunds),
so the order check is essential; merchant revocation should accompany an access-ending
refund. Provider outages do not erase an existing signed lease. Zero-price
coupons and duplicate active subscriptions are not supported by this first
adapter; use separate event licenses for promotions.

The service is prepared for Node 24 behind an HTTPS reverse proxy on an owned
subdomain. DNS names the server; it does not replace hosting. No dependency on
Vercel is required. Public checkout stays disabled until native features,
merchant onboarding and live purchase/renewal/cancel/refund checks pass. The
fixture tests do not establish a successful real-world payment.

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
