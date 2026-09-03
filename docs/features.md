# Functional Specification

## Purpose and status

This document specifies the required user-facing behavior of Duel 6 Reloaded.

Unless a requirement identifies an approved change, the requirements describe the implementation at commit `8f98d3679c4c9091e8973a1cb7a3278f04deb946`.
The split-screen removal requirements define target product behavior that supersedes the earlier implementation baseline.
The Burnable Trees requirements define an approved change to the earlier implementation baseline.
The Rounds field focus and session-memory requirements define an approved change to the earlier implementation baseline.
The round-summary progress requirements define an approved change to the earlier implementation baseline.
The consolidated Teams menu requirements define an approved change to the earlier implementation baseline.
The consolidated person list requirements define an approved change to the earlier implementation baseline.
The Equalize and Shuffle menu requirements define an approved change to the earlier implementation baseline.
The person-action alignment requirements define an approved change to the earlier implementation baseline.
The person-list space and menu-button refinement requirements define an approved change to the merged PR #60 baseline at commit `f2de2ac008ac6282a98acd6c44dc7543e5bfd73c`.

The word **person** means a persistent named record. The word **player** means a person in the active match roster.

A **match** contains consecutive rounds. A **round** contains one level and ends when its selected game mode finds a winner or no winner.

A **playable level** is any level that the application successfully loads.

The **Burnable Trees** setting controls explosion-triggered burn behavior for coniferous and broad-leaved decorative trees.

A **tree-burning explosion** is an explosion that could burn an applicable tree in the documented implementation baseline.

A **random permutation** is one roster order selected at random from all possible roster orders. The existing order is a valid result.

Requirement IDs are stable references. Inventory notes are current observations and are not permanent product requirements.

## Match setup

- **SET-001** The menu must let the user add a person with a non-empty, unique name.
- **SET-002** The menu must ignore an empty name or a name that already exists.
- **SET-003** The menu must let the user remove an available person after the user confirms `Really delete? (Y/N)`.
- **SET-004** The menu must let the user move a person between the available-person list and the player roster.
- **SET-005** The roster must contain a maximum of 15 players.
- **SET-006** The game must refuse to start with fewer than two players.
- **SET-007** After SET-006 occurs, the menu must show `Can't play alone ...` and wait for an input event.
- **SET-008** The game mode selector must show `Deathmatch`, `Predator`, and `Teams`.
- **SET-009** The game mode selector must show `Teams` one time.
- **SET-010** The menu must let the user set Assistance, Quick Liquid, Burnable Trees, and the round limit before a match.
- **SET-011** The round-limit field must accept digits only.
- **SET-012** An empty round-limit field must set the round limit to zero.
- **SET-013** A round limit of zero must mean that the match has no last round.
- **SET-014** A positive round limit must make that numbered round the last round.
- **SET-015** The menu must apply the round-limit field when the user presses Enter in that field.
- **SET-016** The menu must apply the round-limit field again when the user starts a match.
- **SET-017** Shuffle must apply a random permutation to the player roster and move each control assignment with its player.
- **SET-018** Equalize must first order players by descending Elo.
- **SET-019** Equalize must preserve each player's control assignment.
- **SET-020** In Team deathmatch, roster position modulo team count must assign the player's team.
- **SET-021** The roster must show Team deathmatch assignments with the applicable team colors.
- **SET-022** The Clear button and F3 must ask `Really delete? (Y/N)` before clearing statistics for every person.
- **SET-023** Accepting SET-022 must clear every person's non-Elo statistics, preserve Elo data, rebuild the score table, and save person data.
- **SET-024** The main menu must provide a checkbox labeled `Burnable Trees` in the pre-game settings.
- **SET-025** The Burnable Trees checkbox must be checked when the application starts.
- **SET-026** The menu must keep the selected Burnable Trees value during the current application session.
- **SET-027** The application must not save the Burnable Trees value for a later application session.
- **SET-028** When the user starts a match, the game must apply the selected Burnable Trees value to that match.
- **SET-029** The game must keep the applied Burnable Trees value for all rounds in the active match.
- **SET-030** The menu must keep the applied Rounds value during the current application session.
- **SET-031** When gameplay returns to the menu, the Rounds field must show the applied Rounds value.
- **SET-032** The application must not save the Rounds value for a later application session.
- **SET-033** When the Rounds field receives focus and shows exactly `0`, the menu must clear the field.
- **SET-034** When the Rounds field receives focus and shows a positive value, the menu must keep that value.
- **SET-035** When an empty Rounds field loses focus, the menu must set the round limit to zero and show `0`.
- **SET-036** When `Teams` is selected, the menu must show `Num. of Team` and `Friendly Fire` as additional settings.
- **SET-037** When `Teams` is not selected, the menu must not show `Num. of Team` or `Friendly Fire`.
- **SET-038** The `Num. of Team` setting must let the user select two, three, or four teams.
- **SET-039** The `Friendly Fire` setting must let the user select off or on.
- **SET-040** At each application start, `Num. of Team` must default to two teams.
- **SET-041** At each application start, `Friendly Fire` must default to off.
- **SET-042** When the user selects another game mode, the menu must keep the current Team setting values.
- **SET-043** When the user selects `Teams` again, the menu must show the retained Team setting values.
- **SET-044** When gameplay returns to the menu, the menu must keep the selected game mode.
- **SET-045** When gameplay returns to the menu, the menu must keep both Team setting values.
- **SET-046** When a Team setting combination starts a match, the game must use the corresponding existing Team deathmatch rules.
- **SET-047** When the user changes `Num. of Team`, the roster must update the team-color assignments.

### Consolidated person list

The **person list** is the single main-menu list that combines person management and Elo ranking information.

The **person-action row** contains `Remove`, `<<`, and `>>`.

The **roster-order row** contains `Equalize` and `Shuffle` when those actions are visible.

The **batch controller-detection action** detects a control preset for each player in the roster.

- **SET-048** The main menu must replace the separate `ELO Scoreboard` and `Persons` lists with one person list.
- **SET-049** The person list must show each saved person one time, including persons in the player roster.
- **SET-050** For a person with one or more Elo games, the person list must show rank, name, Elo, and Elo trend.
- **SET-051** The person list must put persons with one or more Elo games before persons with zero Elo games.
- **SET-052** The person list must order persons with one or more Elo games by descending Elo.
- **SET-053** The person list must keep persons with zero Elo games in person-record order.
- **SET-054** The person list must not give an Elo rank to a person with zero Elo games.
- **SET-055** The user must be able to select one person in the person list with a pointer click.
- **SET-056** The mouse wheel must scroll the person list when the pointer is inside the list.
- **SET-057** If the selected person is not in the player roster, `>>` must add that person to the roster.
- **SET-058** If the selected person is in the player roster, `>>` must make no change.
- **SET-059** A double-click on a person list row must apply SET-057 and SET-058.
- **SET-060** The `<<` action and a double-click in the player roster must return the applicable player from the roster.
- **SET-061** After SET-060, the person must remain in the person list.
- **SET-062** `Remove` must request the existing delete confirmation only for a selected person that is not in the player roster.
- **SET-063** For a selected person in the player roster, `Remove` must make no change and must not show a confirmation.
- **SET-064** The person-name field and `Add` action must keep the behavior in SET-001 and SET-002.
- **SET-065** The person list must refresh after the user adds or deletes a person.
- **SET-066** The person list must refresh after a roster change, statistics clear, completed match, or saved-data load.
- **SET-067** When the person list refreshes, it must keep the selected person if that person still exists.
- **SET-068** When the selected person no longer exists, the person list must have no selection.
- **SET-069** The consolidation must not change the player roster, control assignment, shuffle, match setting, score table, or footer action behavior.
- **SET-070** The consolidation must not change the existing menu keyboard shortcuts or text-field Enter actions.
- **SET-071** The consolidation must not change controller detection, controller assignment, or controller input behavior.
- **SET-072** The person list must keep saved persons, roster membership, statistics, Elo data, and Elo game counts unchanged across application restarts.

### Team roster order actions

- **SET-073** After SET-018, Equalize must divide the ordered roster into consecutive groups. Each complete group must contain as many players as the selected team count.
- **SET-074** Equalize must apply a random permutation within each group from SET-073.
- **SET-075** When `Teams` is selected, the Players panel must show buttons labeled `Equalize` and `Shuffle`.
- **SET-076** When `Deathmatch` or `Predator` is selected, the Players panel must not show `Equalize` or `Shuffle`.
- **SET-077** A hidden `Equalize` or `Shuffle` button must not have an active interaction target.

### Person action alignment

- **SET-078** The Persons panel must put the person-name field and `Add` in one row.
- **SET-079** In the row from SET-078, `Add` must be to the right of the person-name field.
- **SET-080** The Persons panel must put the person-action row below the person list.
- **SET-081** The person-action row and the roster-order row must have the same vertical centerline.
- **SET-082** The position of the person-action row must not change when `Equalize` and `Shuffle` become visible or hidden.
- **SET-083** This layout change must not change person-list behavior, person actions, roster-order actions, or person-name field behavior.

### Person-list space and menu-button refinement

- **SET-084** The Persons panel must move the person-name row down by one standard list row.
- **SET-085** The person list must use the released space from SET-084 to show one additional standard list row.
- **SET-086** The person-name row must remain above the person-action row.
- **SET-087** `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and the batch controller-detection action must use one common button height.
- **SET-088** Each button in SET-087 must have visible space between its caption and its border on all sides.
- **SET-089** The batch controller-detection action must use the caption `Detect All`.
- **SET-090** The row-level controller-detection actions must keep the caption `D`.
- **SET-091** This refinement must not change person-list content, person actions, roster-order actions, or controller-detection behavior.

## Match and round lifecycle

- **LIF-001** A new match must start in the shared arena view showing the whole level.
- **LIF-002** The game must keep the active roster, selected mode, match settings, and accumulated statistics between rounds.
- **LIF-003** At the start of a match, the game must randomly reorder the supplied level list.
- **LIF-004** In shuffle level selection, each round must use the next level in the reordered list.
- **LIF-005** In shuffle level selection, the game must restart at the first level after it uses the last level.
- **LIF-006** In random level selection, each round must select a level independently and may repeat a level.
- **LIF-007** Each round must independently select a normal or mirrored level orientation with equal probability.
- **LIF-008** A level-defined background must take precedence when its named background exists.
- **LIF-009** If the level has no usable named background, the round must select a random available background.
- **LIF-010** The round must play a round-start sound after it prepares players and mode state.
- **LIF-011** When a mode ends a round, the game must play the game-over sound and start a six-second end delay.
- **LIF-012** During the first second of the end delay, the world and players must continue to update.
- **LIF-013** During the last five seconds of the end delay, the round state must stop its normal update.
- **LIF-014** After a non-final round delay ends, the game must start the next round automatically.
- **LIF-015** After three seconds of the end delay, any key press must start the next non-final round.
- **LIF-016** Before that point, F1 must start the next non-final round.
- **LIF-017** Shift+F1 must start the next non-final round before a winner exists.
- **LIF-018** Escape must leave a completed limited match.
- **LIF-019** Shift+Escape must leave a match at any time.
- **LIF-020** When a round ends, the game must increment the persisted played-round count once.
- **LIF-021** The game must save person data after each round ends.
- **LIF-022** The game must end each player's round state when the user leaves a match.

### Match continuation

- **LIF-023** For a limited match, the game must offer `Resume previous game? (Y/N)` only when the saved count is between zero and the limit.
- **LIF-024** If the user accepts LIF-023, the game must keep saved statistics and the saved played-round count.
- **LIF-025** If the user rejects LIF-023, the game must clear non-Elo statistics and set the played-round count to zero.
- **LIF-026** If no limited match can resume, the game must clear non-Elo statistics and set the played-round count to zero.
- **LIF-027** For an unlimited match, the game must ask `Clear statistics? (Y/N)`.
- **LIF-028** If the user accepts LIF-027, the game must clear non-Elo statistics and set the played-round count to zero.
- **LIF-029** If the user rejects LIF-027, the game must keep saved statistics and the saved played-round count.

## Input and player actions

- **INP-001** Each roster position must have one selected control preset.
- **INP-002** The game must provide six keyboard presets.
- **INP-003** The game must add a control preset for each detected supported game controller.
- **INP-004** A game-controller preset must support the left stick or directional pad for horizontal movement.
- **INP-005** A game-controller preset must support the directional pad or left stick for crouch.
- **INP-006** A game-controller preset must map jump, shoot, pick, and status to controller inputs.
- **INP-007** The menu must let the user select the same control preset for more than one player.
- **INP-008** If multiple control presets match in one detection poll, the menu must assign the last matching preset in the available-preset order.
- **INP-009** Batch detection must rescan controllers and then detect a control preset for each roster position.
- **INP-010** The application must rescan controllers when a controller connects or disconnects.
- **INP-011** Controller connection changes during a round must not automatically reassign a player.
- **INP-012** The seven player actions are move left, move right, jump, crouch, shoot, pick a weapon, and show status. The player must repeat jump to double-jump.
- **INP-013** A player must be stationary on a hard surface or elevator to pick a weapon.
- **INP-014** When a player uses pick while holding a weapon, the player must first drop that weapon.
- **INP-015** The status action must show all player indicators for five seconds.
- **INP-016** Dead players must not respond to player actions unless ghost mode makes them a ghost.
- **INP-017** A ghost must be able to move but must not be able to shoot.

## Player state and world behavior

- **PLY-001** Each player must start a round with full life, full air, and a random enabled weapon.
- **PLY-002** Each player must start with a random ammo quantity inside the configured inclusive range.
- **PLY-003** Each player must start with two seconds of invulnerability.
- **PLY-004** Each player must show name and ammo indicators for four seconds after spawn.
- **PLY-005** The shared view must show a location indicator for the first three seconds of a round.
- **PLY-006** A player must drop the held weapon after death.
- **PLY-007** A player that remains inside a wall for more than five seconds must move to the level's first start position.
- **PLY-008** A living player must regenerate life after six seconds without damage.
- **PLY-009** The regeneration rate must increase with that player's kills in the current round.
- **PLY-010** A player with no current-round kills must not regenerate life.

### Water, elevators, and sudden death

- **ENV-001** Levels may contain solid blocks, waterfalls, three water types, elevators, and decorative content.
- **ENV-002** A player must move at reduced speed while the player's head is underwater without a snorkel.
- **ENV-003** Water must reduce air while the player's head is underwater without a snorkel.
- **ENV-004** Blue, red, and green water must reduce air at progressively higher rates.
- **ENV-005** When air reaches zero, continued water exposure must reduce life.
- **ENV-006** Air must recover at twice its standard recharge rate while the player's head is outside water.
- **ENV-007** Entering water at foot level must create a splash and play that water type's splash sound.
- **ENV-008** A player must be able to stand on and move with an elevator.
- **ENV-009** During sudden death, the water level must rise once every three seconds.
- **ENV-010** In Deathmatch and Predator, Quick Liquid must start sudden death immediately.
- **ENV-011** Without Quick Liquid, Deathmatch and Predator must start sudden death when two of more than two original players remain.
- **ENV-012** In Team deathmatch, Quick Liquid must start sudden death immediately.
- **ENV-013** Without Quick Liquid, Team deathmatch must start sudden death when any configured team has fewer than two living players.

### Decorative tree burning

- **ENV-014** Burnable Trees must apply only to coniferous trees at block index 7 and broad-leaved trees at block index 8.
- **ENV-015** When Burnable Trees is on, a tree-burning explosion that reaches an intact applicable tree must start the existing burn behavior.
- **ENV-016** After ENV-015 occurs, the tree must show its burn animation and then remain in its burned visual state for the round.
- **ENV-017** An applicable tree must burn no more than once in a round.
- **ENV-018** When Burnable Trees is off, explosions must not change applicable trees from their intact visual state.
- **ENV-019** The Burnable Trees value must not make terrain, walls, or other decorative map elements burn or become destroyed.
- **ENV-020** The Burnable Trees value must not change explosion effects on players, shots, terrain, walls, or other world objects.
- **ENV-021** The `shit thrower` explosion must not burn an applicable tree, regardless of the Burnable Trees value.

## Combat, weapons, and pickups

- **CMB-001** A shot must consume one ammo unless the player has Infinite ammo.
- **CMB-002** A player with no ammo and no Infinite ammo must not shoot.
- **CMB-003** Each accepted shot action must add one to the player's shot count.
- **CMB-004** Splitfire must fire one shot in each horizontal direction and add two to the shot count.
- **CMB-005** A direct hit on another vulnerable player must add one to the shooter's hit count.
- **CMB-006** A shot must apply its weapon-specific direct, splash, movement, or status behavior.
- **CMB-007** Shot force must move a player even when invulnerability prevents damage.
- **CMB-008** Fast reload must halve the current weapon's reload interval.
- **CMB-009** Chargeable weapons must charge while the shoot input is held and fire on release when sufficiently charged.
- **CMB-010** A dropped weapon must keep its ammo and remaining reload time.
- **CMB-011** A spawned weapon pickup must contain a random enabled weapon and 10 through 19 ammo.
- **CMB-012** Picking a weapon must transfer the pickup's weapon, ammo, and remaining reload time to the player.
- **CMB-013** If the player's current weapon has ammo, a successful pickup must create the replaced weapon at the player's collider position. The weapon must receive twice the player's horizontal and vertical velocity.
- **CMB-014** If the player's current weapon has no ammo, a successful pickup must remove the selected weapon pickup. The old weapon must remain in the world as a dropped weapon with zero ammo. It must start at the player's collider position and receive twice the player's horizontal and vertical velocity.
- **CMB-015** The game must show `You picked up gun <name>` after a successful weapon pickup.
- **CMB-016** The world must make random attempts to add a bonus or weapon pickup during an active round.
- **CMB-017** A pickup must not spawn in water that has risen to its position.
- **CMB-018** A pickup must not spawn in terrain, on another pickup, or within the protected area around a player.
- **CMB-019** A weapon pickup must spawn on a hard surface.
- **CMB-020** When pickup count reaches the level-dependent limit, the game must remove the oldest pickup of the newly added category.

## Bonuses

- **BON-001** The game must provide Plus life, Minus life, Full life, and Bullets as immediate bonuses.
- **BON-002** Plus life must add a random amount from 14 through 63, subject to the maximum-life limit.
- **BON-003** Minus life must remove a random amount from 14 through 63 unless the player is invulnerable.
- **BON-004** A death caused by Minus life must count as an environmental death and penalty.
- **BON-005** Full life must restore the player to maximum life.
- **BON-006** Bullets must add a random quantity from 5 through 16.
- **BON-007** The game must provide Fast reload, Powerful shots, Invulnerability, Fast movement, Invisibility, Splitfire, Vampire shots, Infinite ammo, and Snorkel.
- **BON-008** A spawned timed bonus must have a duration from 13 through 29 seconds.
- **BON-009** A player must have no more than one timed bonus at a time.
- **BON-010** A different timed bonus must replace the active timed bonus.
- **BON-011** A repeated timed bonus must add half the pickup duration to its remaining and total duration.
- **BON-012** Fast movement must increase movement speed and strengthen the second jump.
- **BON-013** Invisibility must render the player body and weapon at 20 percent opacity.
- **BON-014** Splitfire must apply CMB-004.
- **BON-015** Vampire shots must restore life equal to direct-hit damage, subject to maximum life.
- **BON-016** Infinite ammo must permit shooting without reducing ammo.
- **BON-017** Snorkel must prevent underwater air loss and the underwater movement reduction.
- **BON-018** Invulnerability must prevent shot and environmental damage.
- **BON-019** Powerful shots must apply each weapon's implemented powerful-shot behavior.
- **BON-020** A successful pickup must play the player's bonus-pickup sound and remove that pickup.

## Scoring and assists

- **SCO-001** Total points must equal kills plus wins plus assists minus penalties.
- **SCO-002** Killing another player must add one kill to the killer and one death to the killed player.
- **SCO-003** A player-caused suicide must add one penalty and one death to the player that caused it.
- **SCO-004** An environmental death must add one penalty and one death to the affected player.
- **SCO-005** A same-team kill with friendly fire on must add one penalty to the killer instead of one kill.
- **SCO-006** A same-team shot with friendly fire off must cause no damage.
- **SCO-007** The game must track each attacker's cumulative damage to each target during the round.
- **SCO-008** An attacker must qualify for an assist after causing more than 40 life points of damage to that target.
- **SCO-009** The killer must not receive an assist for the same kill.
- **SCO-010** A player that detonates or shoots down a damaging projectile may receive the resulting assist damage.
- **SCO-011** A confirmed assist must add one assist and the qualifying assisted-damage amount.
- **SCO-012** In Deathmatch and Predator, all qualified non-killer attackers must receive assists for a kill or assisted suicide.
- **SCO-013** In Team deathmatch, an assisted enemy kill must credit qualified players on the killer's team.
- **SCO-014** If Assistance is on, an assisted enemy kill must also credit qualified players on other teams.
- **SCO-015** An enemy killed by the killer's suicide must not award assists.
- **SCO-016** For a same-team kill, qualified attackers from teams other than the killer's team must receive assists.
- **SCO-017** For an assisted suicide in Team deathmatch, only qualified attackers from other teams must receive assists.
- **SCO-018** The game must track shots, hits, kills, deaths, assists, wins, penalties, games, survival time, damage, assisted damage, Elo, and Elo trend.
- **SCO-019** The menu score table must order persons by points, then wins, then damage.
- **SCO-020** The menu must exclude persons with zero games from the main score table.
- **SCO-021** The Elo table must order persons with Elo games by descending Elo.
- **SCO-022** Only a completed, limited Deathmatch must update Elo.
- **SCO-023** Predator, Team deathmatch, and unlimited matches must not update Elo.
- **SCO-024** Clearing statistics must preserve Elo, Elo trend, and Elo game count.

## Game modes

### Deathmatch

- **MOD-DM-001** Deathmatch must end when one player remains alive or no players remain alive.
- **MOD-DM-002** The sole survivor must receive one win and `You have won!`.
- **MOD-DM-003** If no player survives, each player must receive `End of round - no winner` and no win.

### Predator

- **MOD-PR-001** Each round must randomly select one player as the predator.
- **MOD-PR-002** The predator body must use 10 percent opacity.
- **MOD-PR-003** Damage to the predator from a shot must be 30 percent of its normal amount.
- **MOD-PR-004** Each non-predator player must receive 10 additional ammo at round start.
- **MOD-PR-005** If the predator dies and at least one other player remains, all living non-predators must each receive one win.
- **MOD-PR-006** MOD-PR-005 must show `Marines won!` to each living winner.
- **MOD-PR-007** If only the predator remains alive, the predator must receive one win and `Predator won!`.
- **MOD-PR-008** If no player survives, each player must receive `End of round - no winner` and no win.

### Team deathmatch

- **MOD-TM-001** Team deathmatch must use Alpha, Bravo, Charlie, and Delta in that order up to the selected team count.
- **MOD-TM-002** Team assignment must follow SET-020 in every round.
- **MOD-TM-003** The game must apply the team color to the player's trousers, hair top, and headband color.
- **MOD-TM-004** If the profile has no hair or short hair, the game must enable the headband.
- **MOD-TM-005** Team deathmatch must end when all living players belong to one team or no players remain alive.
- **MOD-TM-006** Each living member of the surviving team must receive one win.
- **MOD-TM-007** Dead members of the surviving team must not receive a win.
- **MOD-TM-008** The game must show `Team <name> won!` to all members of the surviving team.
- **MOD-TM-009** If no player survives, each player must receive `End of round - no winner` and no win.
- **MOD-TM-010** Team rankings must total each team's player points and sort teams by points.
- **MOD-TM-011** Team rankings must sort players inside each team by points.

## Status, ranking, and shared arena view

The **shared arena view** is one gameplay view that shows the whole level to all players.

- **UI-001** The game must use the shared arena view for every match.
- **UI-002** The shared arena view must show all players in one gameplay view.
- **UI-003** The game must not divide gameplay into separate player views.
- **UI-004** F2 must not change the gameplay view.
- **UI-005** The menu and match settings must not offer a screen-layout selection.
- **UI-006** The console, configuration, and startup commands must not activate separate player views.
- **UI-007** The game must not save or restore a screen-layout selection.
- **UI-008** The shared arena view must show live ranking when ranking display is on.
- **UI-009** F4 must toggle live ranking display.
- **UI-010** Live ranking behavior must be the same for each supported player count.
- **UI-011** Before a winner exists, Tab must toggle the score summary.
- **UI-012** After a winner exists, the game must show the round summary.
- **UI-013** After the last limited round ends, the game must show the game summary.
- **UI-014** A limited match must show round progress.
- **UI-015** Status and event messages must identify relevant winners, kills, teammates, assistants, deaths, bonuses, and weapon pickups.
- **UI-RND-001** After a non-final round ends in a limited match, the round summary panel must show round progress in a separate top row above its `SCORE` heading strip.
- **UI-RND-002** The round summary panel must use `Rounds: <played>|<total>` for round progress.
- **UI-RND-003** In UI-RND-002, `<played>` must include the round that has just ended.
- **UI-RND-004** In UI-RND-002, `<total>` must equal the configured positive round limit.
- **UI-RND-005** The round summary panel must not show round progress for an unlimited match.
- **UI-RND-006** While the limited-match round summary panel is visible, the shared arena view must hide round progress at the top edge.
- **UI-RND-007** The round-progress row must not overlap or replace the score heading strip.
- **UI-RND-008** When the next round starts, the shared arena view must show its top-edge round progress again.
- **UI-RND-009** The final game summary and the active-round Tab score summary must not show the round-progress row from UI-RND-001.
- **UI-RND-010** The round summary panel must right-align the round-progress label in the panel's top-right corner.

### Split-screen removal scope

- **UI-016** The product must not describe split-screen as an available feature in user-visible text or maintained product documentation.
- **UI-017** The release package must not include an asset that is used only for split-screen presentation.
- **UI-018** Split-screen removal must not reduce the supported roster of two through 15 players.
- **UI-019** Split-screen removal must not remove or change a selectable multiplayer mode.
- **UI-020** Split-screen removal must not change player controls, combat rules, scoring rules, or match progression.

Separate-player camera behavior and split-screen renderer behavior are obsolete product behavior. The specification does not require a replacement camera mode.

## Persistence and profiles

- **PER-001** The game must store person records, roster names, and the played-round count in `data/persons.json`.
- **PER-002** If `data/persons.json` does not exist, the menu must start without loaded persons and must continue.
- **PER-003** The menu must save person data when the menu closes.
- **PER-004** The menu must save person data when a round ends.
- **PER-005** On startup, the menu must restore saved persons, roster membership, statistics, Elo data, and the played-round count.
- **PER-006** A profile directory name must match a person's name exactly to apply that profile.
- **PER-007** A matched profile must supply skin colors, player event sounds, and an optional `script.lua`.
- **PER-008** A profile sound entry with a null value must use the default sound for that event when a default exists.
- **PER-009** A person without a matching profile must use randomized skin colors and default player sounds.
- **PER-010** The random skin must include randomized hair, face, clothing, and optional headband properties.
- **PER-011** Team deathmatch must apply MOD-TM-003 and MOD-TM-004 after profile or random skin selection.

## Console, configuration, and scripting

- **CFG-001** The backquote key must open or close the console in the menu and during a match.
- **CFG-002** While the console is open, text input and discrete context key events must go to the console. The match simulation must continue. Shared held keyboard and controller state may still cause player actions.
- **CFG-003** At startup, the game must enable all implemented weapons before it reads configuration.
- **CFG-004** The game must execute `data/config.script` after it initializes resources, profiles, and the menu.
- **CFG-005** The game must execute command-line console commands after `data/config.script` and in argument order.
- **CFG-006** A later valid setting must take precedence over an earlier setting for the same behavior.
- **CFG-007** The menu's Assistance, Quick Liquid, and Rounds values must take precedence when the user starts a match.
- **CFG-008** The console must support basic clear, echo, list, dump, file execution, alias, and archive commands.
- **CFG-009** The console must support rendering mode, FPS, graphics information, vertical synchronization, volume, rounds, ghosts, level selection, and shot collision commands.
- **CFG-010** The console must support music, controller scan, profile skin, map list, selected-map play, weapon enablement, and starting-ammo commands.
- **CFG-011** The `map` command must reject a value when its parsed index is outside the map list. It must reject a parsed zero unless the supplied value is exactly `0`. A valid nonzero parsed index may have trailing non-numeric text.
- **CFG-012** A valid `map` command must use only the supplied map indexes and must then use the normal match-start validation and prompts.
- **CFG-013** A weapon must be eligible for starting weapons and spawned weapon pickups only when it is enabled.
- **CFG-014** When Lua support is not present, the game must continue without profile scripts.
- **CFG-015** When a profile has no `script.lua`, the game must continue without a script for that profile.
- **CFG-016** When a profile script fails to load, the game must report the script error in the console and continue without that script.
- **CFG-017** A loaded profile script must receive `roundStart`, `roundUpdate`, and `roundEnd` callbacks.
- **CFG-018** A loaded profile script must be able to inspect its person, player, other players, level, shots, weapon, bonus, life, air, ammo, movement, and round kills.
- **CFG-019** Lua must expose `pressLeft`, `pressRight`, `pressUp`, `pressDown`, `pressShoot`, `pressPick`, and `pressStatus`. A script must repeat `pressUp` to request a double-jump.
- **CFG-020** Script-requested actions must use normal player action rules and must not directly edit world state.

## Implemented error, empty, and recovery behavior

- **ERR-001** The menu must use SET-002 for empty or duplicate person names.
- **ERR-002** The menu must use SET-006 and SET-007 for an insufficient roster.
- **ERR-003** For each value that CFG-011 rejects, the console must show `Invalid map index <value>` and must not start the match.
- **ERR-004** Random pickup placement must skip an attempt when the level has no valid position.
- **ERR-005** Profile scripting must use CFG-014 through CFG-016 when scripting is unavailable, absent, or invalid at load time.
- **ERR-006** The current product has no user-facing recovery for malformed required JSON or missing required profile files.
- **ERR-007** The current product has no setup validation for duplicate control ownership, team population, or disconnected assigned controls.
- **ERR-008** Before a continuation prompt, the menu must validate match-start prerequisites for normal Play and a valid `map` command.
- **ERR-009** For normal Play, the requested level set must contain all playable levels that are available to the menu.
- **ERR-010** For a valid `map` command, the requested level set must contain the playable levels at the accepted map indexes.
- **ERR-011** The level prerequisite is satisfied when the requested level set contains one or more playable levels.
- **ERR-012** The weapon prerequisite is satisfied when one or more weapons are enabled.
- **ERR-013** If a match-start prerequisite is not satisfied, the menu must not start the match or show a continuation prompt.
- **ERR-014** The menu must show one blocking report that identifies each match-start prerequisite that is not satisfied.
- **ERR-015** A rejected match-start request must preserve the roster, selected mode, and match settings.
- **ERR-016** A rejected match-start request must preserve all person statistics and the played-round count.
- **ERR-017** The user must be able to dismiss the report and continue to use the menu.
- **ERR-018** The report must tell the user to correct the content or configuration and restart the application.
- **ERR-019** The application must not load corrected content files or configuration files during the current application session.
- **ERR-020** When both prerequisites are satisfied, the validation must not change the existing match-start prompts or match behavior.

ERR-008 through ERR-020 specify the approved safe match-start behavior for implementation.

Other product decisions that are not defined by the implementation are tracked in [GitHub issue #7](https://github.com/mkapusnik-apps/duel6r/issues/7). The requirements in this section remain the source of truth.

## Mutable implementation inventory

This section records current content. Content maintainers may change it without changing the behavioral requirements above.

### Levels

The shipped level inventory contains 28 JSON levels: `resources/levels/duel_01.json` through `resources/levels/duel_28.json`.

`resources/levels/*.json` is the maintainable source for the current shipped level inventory.

### Weapons

The implemented weapon inventory contains 17 weapons in console-index order:

| Index | User-visible name | Shipped default |
| ---: | --- | --- |
| 0 | `pistol` | Enabled |
| 1 | `bazooka` | Enabled |
| 2 | `lightning` | Enabled |
| 3 | `shotgun` | Enabled |
| 4 | `plasma` | Enabled |
| 5 | `laser` | Enabled |
| 6 | `machine gun` | Enabled |
| 7 | `triton` | Enabled |
| 8 | `uzi` | Enabled |
| 9 | `bow` | Enabled |
| 10 | `slime` | Enabled |
| 11 | `double laser` | Enabled |
| 12 | `kiss of death` | Enabled |
| 13 | `spray` | Disabled by `data/config.script` |
| 14 | `sling` | Disabled by `data/config.script` |
| 15 | `stopper gun` | Disabled by `data/config.script` |
| 16 | `shit thrower` | Disabled by `data/config.script` |

`Weapon::initialize` in `source/Weapon.cpp` is the maintainable source for implementation order.

Each weapon definition in `source/weapon/impl` is the maintainable source for its user-visible name and behavior.

`resources/data/config.script` is the maintainable source for shipped enablement overrides.

### Bonuses and water

`BonusType::ALL` in `source/Bonus.cpp` is the maintainable source for the 13 bonus types.

`Water::initialize` in `source/Water.cpp` is the maintainable source for blue, red, and green water.

## Acceptance criteria

- **AC-001** A user can create a valid roster of two through 15 unique persons. The user can start each selectable game mode and Team configuration.
- **AC-002** The menu blocks a one-player start and shows the implemented message.
- **AC-003** Limited and unlimited match starts follow LIF-023 through LIF-029 for resume and clear choices.
- **AC-004** Consecutive rounds follow the configured limit, level-selection mode, mirror selection, end delay, and transition rules.
- **AC-005** Deathmatch produces the winner and no-winner outcomes in MOD-DM-001 through MOD-DM-003.
- **AC-006** Predator applies the role, damage, ammo, and winner rules in MOD-PR-001 through MOD-PR-008.
- **AC-007** Each Team setting combination applies the applicable team assignment, team identity, friendly-fire, scoring, ranking, and winner rules.
- **AC-008** A qualifying attack awards assists according to SCO-007 through SCO-017 in free-for-all and team scenarios.
- **AC-009** Total points, ranking order, penalties, deaths, and Elo behavior follow SCO-001 through SCO-024.
- **AC-010** Each action in INP-012 works through an assigned keyboard or connected game-controller preset. Repeated jump input produces the implemented double-jump. INP-008 defines detection precedence.
- **AC-011** Shuffle and Equalize preserve each player's assigned control preset.
- **AC-012** Starting weapons, ammo, shooting, charge, reload, drops, pickups, and empty-ammo behavior follow PLY-001 and CMB-001 through CMB-020. A replaced weapon must remain in the world at the player's collider position. It must receive twice the player's horizontal and vertical velocity. This behavior must also apply when the replaced weapon has zero ammo.
- **AC-013** Each immediate and timed bonus produces the applicable behavior in BON-001 through BON-020.
- **AC-014** Spawn protection, indicators, water, drowning, elevators, stuck recovery, regeneration, and sudden death follow PLY-003 through ENV-013.
- **AC-015** Each selectable mode uses one shared arena view for each supported player count. The view does not divide into separate player regions.
- **AC-016** A completed round persists person statistics, roster membership, and played-round count for the next menu load.
- **AC-017** A missing person-data file produces an empty usable menu, as specified by PER-002.
- **AC-018** Matched, unmatched, partially defaulted, absent-script, invalid-script, and no-Lua profile cases follow PER-006 through CFG-020. Lua exposes only the seven action callbacks in CFG-019.
- **AC-019** Console input and continued match input follow CFG-001 and CFG-002. Startup configuration and command-line commands apply in CFG-003 through CFG-007 precedence order.
- **AC-020** Console map and weapon controls follow CFG-011 through CFG-013. Map selection accepts a valid nonzero numeric prefix and rejects only the cases in CFG-011.
- **AC-021** The shipped content matches the mutable inventory or the inventory is updated from its named maintainable sources.
- **AC-022** Malformed required JSON and missing required profile files retain the behavior in ERR-006. ERR-001 through ERR-005 and ERR-007 remain unchanged. Selected-map index rejection remains limited to CFG-011.
- **AC-023** F2, the menu, match settings, the console, configuration, startup commands, and saved data do not activate separate player views.
- **AC-024** Live ranking, score summaries, round progress, status, and event messages remain available in the shared arena view as specified by UI-008 through UI-015.
- **AC-025** Product documentation and user-visible text do not present split-screen as an available feature. The release contains no split-screen-only asset.
- **AC-026** Matches support two through 15 players after split-screen removal. Every selectable mode preserves its specified controls, rules, scoring, and progression.
- **AC-027** For normal Play, no playable level blocks the start and identifies the missing level prerequisite. No enabled weapon blocks the start and identifies the missing weapon prerequisite.
- **AC-028** A valid selected-map start applies the same prerequisite validation to its requested level set and the enabled-weapon set.
- **AC-029** When both prerequisites are missing, one blocking report identifies both errors. The menu shows this report before any resume or statistics-clear prompt.
- **AC-030** A prerequisite rejection preserves the roster, selected mode, match settings, all person statistics, and the played-round count.
- **AC-031** After the user dismisses the report, the menu remains usable. The report instructs the user to correct the content or configuration and restart the application.
- **AC-032** Corrected content files or configuration files do not affect prerequisite validation until the user restarts the application.
- **AC-033** With one or more requested playable levels and one or more enabled weapons, normal Play and valid selected-map starts retain their existing prompts and match behavior.
- **AC-034** The main menu shows `Burnable Trees` as a checked pre-game checkbox after each application start.
- **AC-035** With Burnable Trees on, a reaching tree-burning explosion burns each applicable tree once. The tree remains in its burned visual state for that round.
- **AC-036** With Burnable Trees off, the same tree-burning explosion does not change an applicable tree from its intact visual state.
- **AC-037** The selected Burnable Trees value remains in effect across rounds and after a return to the menu during the same application session.
- **AC-038** After an application restart, Burnable Trees is on without regard to the value from the prior application session.
- **AC-039** In both Burnable Trees states, terrain, walls, other decorations, and other explosion effects remain unchanged. The `shit thrower` explosion does not burn applicable trees.
- **AC-040** An applied positive Rounds value remains available after focus changes and after gameplay returns to the menu during the same application session.
- **AC-041** After an application restart, Rounds does not restore the prior session value. Rounds is `0` unless a startup setting changes it.
- **AC-042** Focusing Rounds clears only the displayed value `0`. Focusing a positive value keeps that value.
- **AC-043** If Rounds remains empty when it loses focus, the field shows `0` and the match has no last round.
- **AC-044** The game mode selector shows `Deathmatch`, `Predator`, and one `Teams` option.
- **AC-045** Selecting `Teams` shows `Num. of Team` and `Friendly Fire`. Selecting another game mode hides both settings.
- **AC-046** At each application start, the Team settings are two teams and Friendly Fire off.
- **AC-047** The Team settings support all combinations of two, three, or four teams with Friendly Fire off or on.
- **AC-048** Switching away from `Teams` and back keeps both Team setting values.
- **AC-049** After gameplay returns to the menu, the selected game mode and both Team setting values remain unchanged.
- **AC-050** A change to `Num. of Team` updates roster team colors according to SET-020 and SET-021.
- **AC-051** Each Team setting combination starts Team deathmatch with its selected team count and Friendly Fire value.
- **AC-052** After each non-final round in a limited match, the round summary panel shows `Rounds: <played>|<total>`. The label is in a separate top row above the `SCORE` heading strip. The panel right-aligns the label in the panel's top-right corner. The label does not overlap or replace the heading strip. `<played>` includes the completed round. `<total>` equals the configured round limit. While the panel is visible, the top edge of the arena does not show duplicate round progress. The top-edge progress returns when the next round starts. An unlimited-match summary does not show the panel label. The final game summary and the active-round Tab score summary remain unchanged.
- **AC-053** The main menu has one person list instead of separate `ELO Scoreboard` and `Persons` lists.
- **AC-054** The person list shows each saved person once. It includes roster members and persons with zero Elo games.
- **AC-055** Ranked rows show rank, name, Elo, and Elo trend in descending Elo order. Unranked rows follow them without an Elo rank.
- **AC-056** Pointer selection, wheel scrolling, `>>`, `<<`, `Remove`, `Add`, and both documented double-click actions follow SET-055 through SET-068.
- **AC-057** A roster member cannot be added again or deleted through the person list.
- **AC-058** Person-list refreshes keep a valid selection and clear a deleted selection.
- **AC-059** A match result updates the ranked rows without changing person records or roster membership.
- **AC-060** Statistics clear preserves Elo data and refreshes the person list as specified by SET-023 and SET-066.
- **AC-061** An application restart restores the same persons, roster membership, statistics, and Elo data in the consolidated list.
- **AC-062** The consolidation does not change roster controls, shuffles, settings, score table, footer actions, keyboard shortcuts, text-field Enter actions, or controller behavior.
- **AC-063** Selecting `Teams` shows `Equalize` and `Shuffle`. Selecting `Deathmatch` or `Predator` hides both buttons and removes both interaction targets.
- **AC-064** In `Teams`, Shuffle applies a random permutation to the roster. A Shuffle result may match the prior roster order. Equalize distributes consecutive Elo-ranked groups across the selected team positions. Both actions keep each player's control assignment.
- **AC-065** `Remove`, `<<`, and `>>` stay on a row that aligns vertically with the `Equalize` and `Shuffle` row in every game mode. `Add` is to the right of the person-name field in the same row as that field. All affected controls keep their specified behavior.
- **AC-066** The person-name row is one standard list row lower. The person list shows one additional standard list row in the released space.
- **AC-067** `Remove`, `<<`, `>>`, `Equalize`, `Shuffle`, and `Detect All` have equal heights. Each caption has visible space from the button border on all sides.
- **AC-068** The batch controller-detection action shows `Detect All`. Each row-level controller-detection action still shows `D`.
- **AC-069** The refinement keeps person-list content, person actions, roster-order actions, and controller-detection behavior unchanged.

## Source traceability

The principal behavior sources for this specification are:

- Setup and persistence: `source/Menu.cpp`, `source/Person.cpp`, and `source/PersonList.cpp`.
- Match lifecycle: `source/Game.cpp`, `source/Round.cpp`, and `source/World.cpp`.
- Modes and scoring: `source/gamemodes/*`, `source/PlayerEventListener.cpp`, and `source/Person.h`.
- Player and environment: `source/Player.cpp`, `source/Water.cpp`, and `source/BonusList.cpp`.
- Decorative tree burning: `source/Fire.cpp`, `source/Fire.h`, `source/weapon/LegacyShot.cpp`, and `source/weapon/impl/ShitThrowerShot.cpp`.
- Input: `source/input/PlayerControls.cpp`, `source/input/Input.cpp`, and `source/Application.cpp`.
- Weapons and bonuses: `source/Weapon.cpp`, `source/weapon/impl/*`, `source/Bonus.cpp`, and `source/bonus/*`.
- Console and configuration: `source/ConsoleCommands.cpp`, `source/Application.cpp`, and `resources/data/config.script`.
- Profiles and scripting: `source/PersonProfile.cpp`, `source/PlayerSounds.cpp`, `source/script/*`, and `resources/lua-scripting.txt`.
- User-visible match status: `source/WorldRenderer.cpp` and `source/InfoMessageQueue.*`.

Source traceability identifies where the documented behavior is defined. It is not acceptance evidence that the behavior works at runtime.
