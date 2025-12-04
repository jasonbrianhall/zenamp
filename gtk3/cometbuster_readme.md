# Comet Buster - Game Documentation

## Overview

**Comet Buster** is a high-energy arcade-style space shooter where you pilot the experimental mining vessel *Destiny* through treacherous asteroid fields while battling rival factions, mining comets, and competing for survival in the most dangerous sector of known space.

---

## The Story: Why You're Out Here

### The Asteroid Field Crisis

The Kepler-442 Asteroid Field was once a stable mining zone, rich with valuable minerals and rare isotopes. Then something changed. The asteroids began fragmenting at unprecedented rates, creating a chaotic debris field that standard mining operations couldn't handle.

Three factions saw opportunity. Three factions saw profit. And three factions sent their best ships into the chaos.

### Enter: The Destiny

You pilot the **Destiny**, an experimental all-purpose mining and combat vessel developed by independent deep-space miners. It's not the fastest ship. It's not the most armored. But it's *yours*, and it's equipped with rapid-fire cannons, advanced targeting systems, and an AI that refuses to give up.

Your mission: Survive long enough to extract valuable resources, compete against corporate and military forces, and discover the source of the asteroid field destabilization.

**What you didn't expect:** You wouldn't be alone.

---

## The Factions

### 🔴 RED SHIPS: The Military Hunter-Class Vessels

**Faction:** Galactic Defense Collective (GDC)
**Designation:** Type-7 Aggressive Interceptors
**Color:** Bright Red

#### Their Mission
The Red Ships are military enforcement vessels sent to "maintain order" in the asteroid field. Officially, they're here to prevent illegal mining and ensure sector stability. Unofficially? They're trying to monopolize the region's resources.

#### Their Tactics
- **Relentless Pursuit** - Red Ships actively hunt anything that moves, including you
- **Direct Combat** - They shoot frequently and accurately at player targets (every 0.3-0.8 seconds)
- **No Negotiation** - They don't distinguish between miners and competitors; if you're there, you're a threat
- **Asteroid Indifference** - While you struggle with asteroids, Red Ships ignore them completely

#### Why They're Here
The GDC heard about the asteroid field crisis and saw a chance to assert military dominance over a valuable region. They claim they're maintaining peace. They're actually trying to claim the sector for themselves.

#### In-Game Behavior
- Hunt the player (YOU) aggressively
- Ignore asteroids completely
- Shoot frequently and without mercy
- Consider your presence a threat to their operations

---

### 🔵 BLUE SHIPS: The Peacekeeper Patrol Vessels

**Faction:** Independent Sector Alliance (ISA)
**Designation:** Type-4 Patrol Craft
**Color:** Blue

#### Their Mission
Blue Ships are peacekeepers and asteroid field monitors sent by the Independent Sector Alliance. Their official role: make space safer by clearing hazardous asteroids and maintaining order without military aggression.

#### Their Tactics
- **Constructive Cooperation** - Blue Ships actually help by destroying asteroids
- **Protective Patrol** - They follow peaceful sine-wave patrol patterns
- **Smart Targeting** - They prioritize nearby asteroids and destroy them methodically
- **Measured Firepower** - They shoot less frequently (0.8-1.8 seconds) to conserve energy

#### Why They're Here
The ISA believes the asteroid field destabilization is a natural phenomenon that affects everyone. They sent ships to help stabilize the region and protect independent miners like you. They're the "good guys," and they actually mean it.

#### In-Game Behavior
- Follow patrol patterns peacefully
- Actively shoot and destroy asteroids
- Will explode if they collide with asteroids (they're brave, not suicidal)
- Bullets destroy comets as well
- Generally ignore the player unless you hit them first
- If a collision happens: mutual destruction (they go down with honor)

---

### 🟢 GREEN SHIPS: The Corporate Mining Security Drones

**Faction:** Asteroid Mining Collective (AMC)
**Designation:** Type-9 Corporate Security Interceptor
**Color:** Bright Green

#### Their Mission
Green Ships are the automated mining drones of the Asteroid Mining Collective, a mega-corporation that views the asteroid field as prime real estate for extraction and profit. They're programmed with one purpose: maximize mineral extraction while protecting corporate interests.

#### Their Tactics
- **Efficient Asteroid Destruction** - They blast through asteroids at incredible speed with superior targeting (0.15-0.4 seconds)
- **Territorial Enforcement** - They have a 300-pixel "restricted zone" around their current position
- **Aggressive Defense** - If you breach their territory, they WILL chase you down with extreme prejudice
- **Corporate Focus** - They ignore everything except asteroids and threats to their operations

#### Why They're Here
The AMC doesn't care about stability or peace. They see the asteroid field as a gold mine (literally) and want to strip-mine it for all it's worth. Green Ships operate with ruthless efficiency and absolutely zero sympathy for other miners.

#### The Controversy
The AMC officially claims their ships are "civilian mining drones," but their combat AI is suspiciously aggressive for mere mining equipment. Independent miners suspect the AMC is intentionally trying to drive other factions out of the sector through intimidation.

#### In-Game Behavior
- Ignore the player completely while patrolling
- Rapidly destroy asteroids (competitive threat)
- **Chase and attack if you get within 300px** (personal space violation = automatic hostility)
- Shoot at comets very quickly with rapid-fire bursts
- Return to normal patrol angle once you escape their range
- Consider you a "threat to mining operations" rather than a military target

---

## The Comet Phenomenon

### What Are These Giant Asteroids?

Within the asteroid field, massive comet-class fragments occasionally drift through. These are larger, more dangerous, but incredibly valuable. They contain rare mineral concentrations that command premium prices on the open market.

### Comet Classification

**MEGA Comets** (radius 50px) - Rare, extremely valuable
- Break into 3 Large Comets when destroyed
- Award 500 points
- Require intense firepower to eliminate

**LARGE Comets** (radius 30px) - Common
- Break into 2 Medium Comets when destroyed
- Award 200 points

**MEDIUM Comets** (radius 20px) - Very common
- Break into 2 Small Comets when destroyed
- Award 100 points

**SMALL Comets** (radius 10px) - Abundant
- Destroyed completely
- Award 50 points

### Color-Coded Frequency Bands

Comets are categorized by their mineral frequency signatures:

- **Red Comets** (Bass Frequency) - Iron and heavy metals
- **Yellow Comets** (Mid Frequency) - Rare earth elements
- **Blue Comets** (Treble Frequency) - Exotic crystalline materials

Each faction has different targets based on their resource needs.

---

## Gameplay Mechanics

### The Destiny's Arsenal

Your ship comes equipped with:
- **Primary Cannons** - Rapid-fire targeting system for destroying asteroids and comets
- **Advanced Thrusters** - Boost your speed with a limited fuel system
- **Omnidirectional Fire** - Middle-click for simultaneous shots in all directions (emergency defense)
- **Scoring Multiplier System** - Build combos for higher scores (resets on damage)

### Survival Systems

- **Lives System** - You start with 3 lives
- **Invulnerability Frames** - Brief protection after taking damage
- **Extra Lives** - Earn bonus lives every 10,000 points
- **Fuel Management** - Monitor your boost fuel to avoid being stranded

### Wave Progression

Each wave increases in difficulty:
- More asteroids spawn
- Asteroids move faster
- Spawn rates increase
- Enemy ships become more aggressive
- The competition intensifies

### Scoring

- **Base Points** - Varies by comet size and type
- **Score Multiplier** - Builds up to 5.0x with consecutive hits
- **Bonus Multiplier** - Extra lives awarded at score milestones
- **Leaderboard** - Top scores saved for bragging rights

---

## Faction Dynamics

### The Three-Way Cold War

The three factions are technically not at war with each other, but tensions run incredibly high:

| Aspect | Red Ships | Blue Ships | Green Ships |
|--------|-----------|-----------|------------|
| **Primary Target** | You | Asteroids | Asteroids |
| **Attitude Toward You** | Hostile | Neutral/Helpful | Territorial |
| **Asteroid Clearing Speed** | Ignore | Slow & methodical | Fast & efficient |
| **Reliability** | Predictable aggression | Stable cooperation | Unpredictable aggression |
| **Corporate Interests** | Military dominance | Independent peace | Profit maximization |

### Why They Coexist

Despite the tension, none of the factions want all-out war:
- **Red Ships vs Green Ships** - Neither faction wants direct combat (military vs corporate stalemate)
- **Red Ships vs Blue Ships** - The military tolerates peacekeepers as "lesser threat"
- **Blue Ships vs Green Ships** - Green Ships view Blue Ships as inefficient and ignore them
- **All vs You** - You're the wild card they didn't expect

---

## Tips for Survival

### Dealing With Each Faction

**Against Red Ships:**
- Use your speed advantage to evade
- Keep moving in unpredictable patterns
- Don't engage directly if you can avoid it
- They're relentless but predictable

**With Blue Ships:**
- Treat them as allies (they won't attack you first)
- Stay out of their way while they clear asteroids
- Use them as blocking obstacles against Red Ships
- Mutual collision = mutual destruction (respect their space)

**Against Green Ships:**
- Keep your distance (maintain >300px range)
- Let them clear asteroids then steal the fragments
- If you get too close, prepare to evade their full assault
- Use their rapid firing to your advantage by timing escapes

### Resource Management

- **Fuel** - Use boost strategically, not constantly
- **Lives** - Prioritize survival over aggressive scoring
- **Score Multiplier** - Don't take unnecessary risks trying to build combos
- **Positioning** - Stay centered and aware of all threats

### Wave Strategy

- Focus on destroying smaller asteroids first
- Save Mega Comets for when you have multiplier bonus
- Use each faction's behavior to your advantage
- Learn patrol patterns of Blue and Green Ships

---

## The Ultimate Goal

Survive the asteroid field chaos. Climb the leaderboard. Discover why the asteroids are destabilizing. And maybe, just maybe, uncover a conspiracy that goes deeper than any of the three factions realize.

The asteroid field isn't just a mining zone. It's a battleground. And the Destiny? The Destiny is the only ship nimble enough, tough enough, and independent enough to navigate it all.

---

## Game Statistics

- **Three Ship Types** - Red (aggressive), Blue (helpful), Green (territorial)
- **Comet Sizes** - 4 types from MEGA to SMALL
- **Maximum Comets Onscreen** - 32
- **Maximum Enemy Ships** - 4
- **Maximum Bullets** - 128
- **Wave Scaling** - Difficulty increases each wave
- **Score Multiplier Range** - 1.0x to 5.0x

---

## Author Notes

Comet Buster is inspired by classic arcade space shooters, but with modern gameplay mechanics and a deeper narrative context. Every faction serves a purpose. Every ship type has unique behavior. And every decision in the asteroid field matters.

The story is yours to write. The Destiny is waiting. The asteroid field is calling.

Survive. Score. Ascend the leaderboard.

Welcome to Comet Buster.

---

*"It's not just about mining asteroids. It's about surviving the competition."* - Captain's Log, The Destiny
