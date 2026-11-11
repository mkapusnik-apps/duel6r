# NET-10 — Mixed session browser

Functional authority: [network-browser](../../screens/network-browser.md), [host-directory](../../network-host-directory.md), especially NET-DIR-013/014/027/032 and NET-PASS-001–018, and product-owned NET-ADM/NET-SOLO. Presentation retains mixed Hosting, common Password, and bound selection in both hosting modes. Product owns phase decoding, eligibility, the common encrypted channel, supported clients, waiting admission, and final closure.

Structural sources: [NET-10 results](../wireframes/NET-10/NET-10.svg), [NET-10-E empty](../wireframes/NET-10/NET-10-E.svg), and [NET-10-U failed refresh](../wireframes/NET-10/NET-10-U.svg).

States retain NET-10-loading, NET-10-results, NET-10-empty, NET-10-stale, NET-10-unavailable, NET-10-dedicated-idle, and NET-10-dedicated-reserved. NET-10-dedicated-unsupported is retired for the four approved baseline clients; do not reuse it for ordinary full, stale, busy, closed, or security failure. Product owns its retirement in the functional inventory. No product screen is added. The layout refinement removes the two static browser captions, enlarges the list, frames selected details, and lowers paging and the single footer row. Discovery, selection, eligibility, joining, and recovery remain unchanged; NET-03 copy is outside this refinement.

## Presentation and allocation

The [unified presentation](../../design.md#unified-network-presentation) applies. Inline empty/error presentation, upper Refresh, and the single-row footer supersede the grey list replacement and lower Refresh/two-row footer. Six columns, selected details, reserved paging, and bound setup remain.

The Browse window is the existing raised panel at (24, 24), 802 by 544 logical pixels, not the surrounding 850 by 700 menu canvas. Keep that panel and its title chrome unchanged. Body components must stay at least 8 logical pixels inside its outer left, right, and bottom edges: four more than the former 4-pixel inset. This leaves 6 pixels inside its 2-pixel bevel, or 4 pixels when a 2-pixel outer focus keyline is present. Keep the existing top allocation below the title; add no new panel, decorative space, or text-size change.

- **UX-NET-10-001** BROWSE SESSIONS must remain above the reserved directory-feedback allocation without `Sessions listed` on successful results.
- **UX-NET-10-002** The table must retain its inset white body and fixed grey heading strip.
- **UX-NET-10-003** Row selection must retain its selection fill/text, textual marker, and separate focus outline.
- **UX-NET-10-004** The six headings must be Session / endpoint, Hosting, Players, Phase, Access, and Admission.
- **UX-NET-10-005** Selected details must remain below the table and above paging with room for full endpoint/identity and eligibility. The read-only region must have a thin black border, at least 8 logical pixels of inner padding, and a 12-logical-pixel gap below the list. The border must not imply an action or add a focus stop.
- **UX-NET-10-006** Pagination must show both framed directions when either direction exists and hide both when neither exists.
- **UX-NET-10-007** The current page label must not invent a total and must hide with unnecessary pagination.
- **UX-NET-10-008** The footer must use the single-row geometry and action order defined below.
- **UX-NET-10-009** The table must lose visible rows before details, paging, or footer overlap.
- **UX-NET-10-010** Loading, empty, stale, and unavailable feedback must retain their owning allocation.
- **UX-NET-10-011** Session / endpoint must retain the largest column allocation.
- **UX-NET-10-012** Long row endpoints must clip inside that column while selected details expose their complete validated value through wrapping or existing scrolling.
- **UX-NET-10-013** Full selected session identity and mode must remain separately labeled and accessible without smaller text or split UTF-8 characters.

Approximate column widths remain 256, 112, 72, 104, 96, and 144 logical pixels. Preserve standard metrics, fixed-height rows, full-row pointer targets, and the table/details relationship. A row count must not take precedence over required detail readability.

On the existing logical canvas, inset the heading strip to (32, 490), 786 by 20, and the list body to (32, 248), 786 by 242. Preserve ten visible rows at the existing 22-logical-pixel pitch, all six headings, and the current column hierarchy. Only edge space narrows; do not reduce text or row height. Place the detail border at (32, 108), 786 by 128. Keep its 8-pixel inner padding and 12-pixel list separation; move its text origin 4 pixels right and fit wrapped values to the narrower inner width. Necessary eligibility explanations and ongoing/final-round context must stay inside the padding without covering paging or actions. These bounds apply to results, empty, and unavailable variants; hidden selected metadata must remain hidden.

## Upper Refresh and single-row footer

These values use the existing 850 by 700 logical canvas and bottom-left renderer origin. SVG coordinates are top-left structural allocation.

- **UX-NET-10-042** Refresh must occupy (706, 518), 112 by 24 logical pixels: 4 pixels left of its former position, with unchanged vertical allocation and size.
- Refresh must retain right edge 818 and an 8-logical-pixel gap above header top 510.
- Its 2-logical-pixel outer focus keyline must retain 6 logical pixels of clearance from the header and title strip at 550.
- Loading and age-stale feedback must move their left origin 4 logical pixels right and keep at least 8 logical pixels of clearance from the Refresh focus keyline.
- The footer under UX-NET-10-008 must show Join selected, Direct connect, and Back at (32, 32), (300, 32), and (568, 32), respectively. It remains the lowered single row, moved up only 4 pixels for the bottom inset.
- Each footer action must measure 250 by 32 logical pixels with 18-logical-pixel gaps, fitting the inset width without reducing caption size.
- Captions must remain centered at the standard 16-logical-pixel text size.
- Whole button rectangles must remain pointer targets with current pressed, disabled, focus, and canvas-scaling behavior.
- The former lower Refresh action must not remain as a separate affordance or activation target. Direct connect must occupy its approved single-row footer bounds. Conditional paging must keep the 8-pixel gap above that row: Previous page at (32, 72) and Next page at (568, 72), each 250 by 24, with the centered current-page label between them moved up 4 pixels. Former bounds must not retain duplicate action targets.

Forward traversal must cycle Refresh → table → Previous page → Next page → Join selected → Direct connect → Back → Refresh. Reverse traversal must reverse this cycle. Both must skip hidden or disabled browser controls under existing eligibility.

Initial results focus remains on the table. Completed empty/error focus remains on Refresh when enabled. Pending initial-read focus remains on Direct connect. This browser-specific cycle must not change focused-disabled retention on other screens.

## Mixed row content

- **UX-NET-10-014** Hosting must show Player-hosted or Dedicated from validated classification rather than infer it from an address or password flag.
- **UX-NET-10-015** Access must show No password or Password from validated none/password metadata consistent with passwordRequired for either hosting mode.
- **UX-NET-10-016** Access must not disclose passwords, owner tokens, or reconnect credentials.
- **UX-NET-10-017** Hosting and Access must not imply verified endpoint identity, certificate validation, or universal reachability.
- **UX-NET-10-018** Players must show authoritative advertised count and effective capacity as count / limit, including reserved slots.
- **UX-NET-10-019** A prepared empty dedicated session must show Idle and 0 / limit without inventing participants, a controller, or a round.
- **UX-NET-10-020** Dedicated idle details must omit `Ready empty session. Your role is confirmed only after admission.` without replacement text. This omission must not imply a role assignment or change dedicated idle behavior.
- **UX-NET-10-021** Controller reservation must show Reserved and truthful occupancy while Admission follows separately advertised eligibility.
- **UX-NET-10-022** Phase must show the product-decoded Lobby, Round 1, Started, Idle, Reserved, or Cleanup context independently from Admission.
- **UX-NET-10-023** Admission must show Open, Full, Closed, Busy, or Unconfirmed from approved availability.
- **UX-NET-10-024** Cleanup/busy details must explain the temporary block without service-administration or controller-election actions.
- **UX-NET-10-025** Unknown or unvalidated metadata must not default to Player-hosted, No password, or Open or open a credential task.

Retired Invite metadata must not be relabeled Password or No password. Product owns safe exclusion, fail-closed page handling, and legacy capability isolation. Action-layout changes authorize no protocol adapter or security fallback.

## Selection and recovery

- **UX-NET-10-026** Join selected must open the applicable bound NET-03 task without connecting or reserving authority.
- **UX-NET-10-027** Selected dedicated rows must enter NET-03-B with bound hosting/protection, endpoint, and session identity.
- **UX-NET-10-028** Selected player-hosted rows must enter NET-03-E with the same read-only target/binding requirements.
- **UX-NET-10-029** Linux x86-64, Windows x86-64, Linux AArch64/Pi 5, and experimental Apple Silicon macOS 14+ clients must show the same eligible Join selected action for both hosting types.
- **UX-NET-10-030** Dedicated details must omit the former Mac/Pi unsupported reason and any replacement platform-warning step.
- **UX-NET-10-031** Stale or expired selection must require successful refresh and must not transfer binding to Direct connect.
- **UX-NET-10-032** Refresh must retain selection by the approved immutable listing/session identity rather than row position.
- **UX-NET-10-033** Same-identity phase changes must update details without claiming replacement or role assignment.
- **UX-NET-10-034** A removed or replaced selection must not select or join a substitute automatically.
- **UX-NET-10-035** Back from selected setup or failure must restore the browser without changing to another selected row.
- **UX-NET-10-036** Independent Direct connect must clear selection binding and secrets before unbound setup.

Protected selection initially focuses Password under UX-NET-03-063. No password selection initially focuses local setup under UX-NET-03-064 while Password remains optional and reachable. Both hosting types use the same encrypted connection; browser labels do not establish authenticated host identity. Hosting remains visible for lifecycle and role comprehension, not as a crypto choice. Password correction must preserve bound setup in both modes.

Full, closed, busy, and age-stale successful results remain inspectable. Failed reads hide cached rows/details. Necessary disabled explanations remain; a password requirement or the former dedicated platform exclusion must not disable otherwise eligible joining on an approved client. This client parity does not add Intel macOS or expand dedicated hosting platforms.

## Inline states and hidden input

- **UX-NET-10-037** Completed empty/error reads must retain all six headings over the existing inset white body and show one complete noninteractive description in standard 16-logical-pixel dark-grey #595959 text with at least 4 logical pixels of horizontal padding and contrast of at least 4.5:1.
- **UX-NET-10-038** Failed reads must hide cached rows and selected identity, endpoint, mode, hosting, and protection details without exposing a browser join target.
- **UX-NET-10-039** Hidden pagination and noninteractive empty/error bodies must have no pointer, wheel, keyboard, or controller input target.
- **UX-NET-10-040** If browser focus becomes unavailable or hidden, it must move to inspectable results, otherwise enabled Refresh, otherwise Direct connect.

Empty/error bodies must show no session rows, body dividers, separators, scrollbar, selection fill/marker, hover, focus, or action frame. Header dividers remain confined to the heading strip. The description must not wrap, truncate, or shrink. Status and paging spaces remain reserved.

| Existing state | Presentation |
|---|---|
| NET-10-results | Six-column table/details; no successful-load status; conditional pagination |
| NET-10-empty | `No active sessions listed. You can host a session or connect directly.` once in the white body; successful no-selection reason blank |
| NET-10-unavailable | `Directory unavailable. Direct connection and local play are still available.` once in the white body; cached rows/details hidden; necessary unavailable explanation retained |
| NET-10-stale | Age-stale successful rows inspectable; Admission Unconfirmed; successful refresh required |
| NET-10-loading | Loading or refreshing in the reserved status region; no false empty/error outcome; joining disabled |

An empty intermediate page may use `No active sessions on this page. Continue with Next page.` Successful current populated results without selection and successful zero-row results must leave the reason line blank without a replacement instruction. Join selected remains disabled. Loading, unavailable, full, closed, stale, expired, and removed-selection explanations remain when applicable.

A generic Selected session label is permitted without retained metadata. A retry after failure must not expose cached selected details before success. Recovery presents retained selection only when its validated identity/session remains present; product owns that decision.

## Ongoing context

- **UX-NET-10-043** Known later-match details must show `Match in progress. Your players join next round if admitted.`; known final-round details must show `Final round in progress. You may not play in this match if admitted.`; unknown context must not promise immediate spawn.
- Selected details must retain full endpoint, identity/mode, protection meaning, and necessary state-dependent feedback. Every browser state must omit `The host confirms availability when you connect.` without replacement text. Authority still follows the functional contract; bound NET-03 guidance remains unchanged.
- **UX-NET-10-046** The browser must omit `A listing does not guarantee reachability.` in every state without replacement text.
- The existing `LAN-first.` text and vertical position must remain unchanged; the common horizontal inset applies to its placement only, without introducing absent copy.
- Context must wrap or use existing scrolling before it covers paging or the footer.
- **UX-NET-10-044** The results representative must pair a selected protected ongoing Open session with a genuinely completed Closed session and mixed-hosting context.
- **UX-NET-10-045** Started or Password must not imply Closed; joining must follow product-authorized Admission and existing freshness/capability checks.

Product determines eligibility and the confirmed lobby, immediate-play, Connected-waiting, or rejection destination. The UI must not infer that decision from Phase, capacity, a listing label, or a local timer.

## Requirement reference compatibility

Each identifier has one canonical meaning in this file.

| Imported develop reference | Canonical composed requirement |
|---|---|
| UX-NET-10-011 inline states | UX-NET-10-037 |
| UX-NET-10-008 upper Refresh | UX-NET-10-042 |
| UX-NET-10-012 single-row footer | UX-NET-10-008 |
| UX-NET-10-013 ongoing details | UX-NET-10-043 |
| UX-NET-10-014 representative | UX-NET-10-044 |
| UX-NET-10-015 phase/admission | UX-NET-10-045 |

## Acceptance

Check both browser traversal directions with keyboard/controller and scaled pointer targets at 850 by 700, 1280 by 720, and 1920 by 1080. Check unchanged Refresh caption/status/focus clearance, the taller list and its last visible row, padded detail containment, equal lowered footer sizes/gaps, conditional paging clearance, and complete inline descriptions. Necessary reasons and long selected values must remain readable; the removed captions must not leave replacement instructions or new input targets.

Check single/multiple pages, refresh-driven pagination changes, hidden input exclusion, blank successful no-selection reasons, failed populated refresh, recovery identity present/absent, and age-stale inspection. Check mixed Password/No password, dedicated Idle/Reserved/Cleanup, full/closed states, long context, and independent Direct. On each approved client, check pointer, keyboard Confirm, and controller Confirm entry to eligible dedicated setup without the retired platform block.

Both selected hosting modes must retain immutable targets and protection-aware Password focus. All joining paths must use the same validated availability, freshness, identity, capability, and admission checks. Stills do not establish those behaviors, security, leases, or lifecycle authority.

The [current inner-padding matrix](../screenshots/README.md#net-10-current-inner-padding-matrix) owns the affected NET-10 results, NET-10-E empty, and NET-10-U unavailable representatives at 1280 by 900. Reuse the prior dedicated-idle supplement for its unchanged copy/state scope, not as evidence of the new outer inset. Do not refresh gameplay, NET-03, or historical unsupported-platform images for this layout-only refinement.
