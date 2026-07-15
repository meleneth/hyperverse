# HUD Design

## Principle

Juice all the HUD to the maximum.

The HUD is not a thin overlay. It is the player's operational interface, combat language, mining instrument panel, warning system, and competence fantasy.

It should be dense without becoming unreadable.

## HUD Responsibilities

The HUD must communicate:

- current ship operational mode
- current control mapping
- movement intent
- aim directions
- camera relationship
- target lock
- asteroid composition
- scan confidence
- ore location
- structural stress
- heat
- volatile gas
- radiation
- collision prediction
- projectile threats
- raider priorities
- drone state
- cargo value
- contract quota
- extraction authorization
- over-quota bonus
- escalation timer
- convoy integrity
- individual cargo box state
- tow status
- stolen-box escape progress

## Stolen Cargo Presentation

Each stolen box should receive a loud tactical presentation including:

- raider identity
- cargo box identity
- cargo contents
- cargo value
- wrapped distance from player
- escape threshold
- distance remaining
- predicted time to loss
- intercept vector
- suggested intercept point
- raider health
- tow stress
- current escape direction
- warning escalation as loss approaches

The HUD should never be subtle about an expensive box leaving the player's effective recovery envelope.

## Tow Presentation

When towing, display:

- box mass
- relative angle
- relative velocity
- current tow force
- coupling stress
- safe maneuver envelope
- projected break time
- break direction
- cargo-specific risk

Warnings should intensify before the tow breaks.

## Control Context Presentation

Any mode change that alters control meaning must be immediately obvious through a combination of:

- HUD label
- input glyphs
- reticle change
- audio
- animation
- haptics
- color or shape changes
- brief transition messaging

The player must never wonder why a stick changed function.

## Camera Composition

Target composition:

- ship horizontally centered
- ship approximately three-quarters down the screen
- forward space visible above
- tunable positional dead zone
- smooth positional catch-up
- tunable rotational lag
- no absolute north-up assumption

## Interaction

Initial HUD interaction should prioritize target-and-context behavior rather than a free cursor during high-speed combat.

Likely interactions:

- lock target
- cycle target sub-elements
- mark mining priority
- mark drone priority
- inspect hazard
- confirm extraction
- select cargo or raider threat
- choose contextual action

A slower command mode may later expose richer direct HUD navigation.
