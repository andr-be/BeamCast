# BeamCast UT Simulator Spec

## USER PROMPT

I want to build a simple, lightweight, visual and mathematically accurate 2d industrial, standard, A-Scan & grit manual ultrasonics simulator that runs with minimal dependencies on Windows (and maybe Linux if it's not utterly laborious?)
the level of art I'm aiming for is entirely possible using primitives, but the aesthetic has to be neat and be able to switch from dark to light mode^0
I'm inspired by creators such as 3blue1brown and Sebastian Lague in an aesthetic and pedagogical sense
the purpose would be to accurately^1 simulate ultrasonics to model interactions that sometimes I have to draw out manually
one of those pedagogical levels would be like, raycasted pulses with back-calculated times of flight at different velocities to model things like impedance matching
it would be interesting to see if you could model 'a-scan' returns by simulating amplitude on pulses that interact with transducer objects
also like, temperature of the material affecting how fast and quickly attenuated the pulse is getting due to the acoustoelastic effect and such
a lot of the modelling is done in textbooks as simple algebraic interactions; Krautkramer's Ultrasonic Testing of Materials is standout

0: obviously this doesn't necessarily need to be incredible, even just inversion from a nice standard whiteboard 5 colour design would probably be enough, but I think of the idea of all colours being something that could be updated without too much hassle as a good architectural consideration
1: accuracy in this sense would be 'as good or better as I could do with a ruler, compass and a calculator and notepad, not molecular dynamics lmao'

---

## SPECIFICATION

### 1. PROJECT OVERVIEW

**Name:** BeamCast UT Simulator
**Purpose:** Educational and professional 2D ultrasonic testing simulator for modeling wave propagation, A-scan generation, and material interactions
**Target Platforms:** Windows (primary), Linux (if practical)
**Aesthetic:** Clean, pedagogical visualizations inspired by 3Blue1Brown and Sebastian Lague
**Accuracy Goal:** "Ruler, compass, and calculator" level - algebraic/geometric accuracy suitable for engineering analysis, not molecular dynamics

### 2. TECHNICAL STACK

**Core Technologies:**
- **Language:** C++ (learning opportunity, embedded systems applicability)
- **Graphics:** SDL2 (minimal dependencies, cross-platform)
- **Build System:** CMake (cross-platform support)
- **Precision:** Double-precision (64-bit) floating point for physics calculations
- **Rendering:** Immediate mode (redraw per frame) - simpler mental model, adequate performance for use case

**Data Storage:**
- Material properties: External JSON files (materials.json)
- Simulation setups: JSON export/import
- Example scenarios: JSON library files
- A-scan waveforms: Exportable as byte arrays with time sequences (future B-scan support)

### 3. PHYSICS MODELING

#### 3.1 Wave Types
- **Longitudinal (L) waves:** Full implementation
- **Shear (S) waves:** Full implementation
- **Mode conversion:** Complete mode conversion at interfaces using unified angle-dependent formulation
- **Surface waves:** Deferred to future (architecture should allow extension)
- **Edge diffraction:** Simulation settings toggle - start simple, allow complex physics later

#### 3.2 Wave Propagation
- **Method:** Ray tracing with discrete ray markers
- **Multi-bounce:** User-configurable amplitude cutoff slider (0-100%)
  - Alternative: Range + Delay cutoff (toggle between modes)
  - Stop tracing paths exceeding A-scan time window or falling below amplitude threshold
- **Velocity:** Material-dependent, temperature-affected (continuous curves)
- **Beam spreading:**
  - Selectable fidelity levels (architectural support for extension)
  - Models: point source → finite aperture → near/far field
  - Near/far field transition: Subtle boundary indicator (N = D²f/4c)
  - Focal zones: Auto-calculated from probe specs (focused transducers)

#### 3.3 Attenuation
- **Baseline:** Simple dB/mm coefficient (initial implementation)
- **Architecture:** Toggle between attenuation models (simulation settings)
  - Constant coefficient
  - Frequency-dependent (α·f^n)
  - Material-specific scattering models (future)
- **Bandwidth effects:** Affects both pulse shape AND frequency-dependent attenuation

#### 3.4 Interface Physics
- **Reflection coefficients:** Unified formulation for all angles (handles normal incidence as special case)
- **Transmission:** Full transmission coefficient calculation
- **Mode conversion:** Calculate all four modes (L-reflected, L-transmitted, S-reflected, S-transmitted)
  - User toggles visibility of individual modes
  - Automatic filtering by amplitude threshold (slider 0-100%)
- **Impedance:** Acoustic impedance Z = ρc calculated per material
- **Couplant layer:** Model thin couplant between transducer and part (v1 priority)
- **Wear plates/delay lines:** Deferred to future
- **Surface roughness:** Simple roughness parameter affecting coupling efficiency and scatter

#### 3.5 Temperature Effects
- **Velocity:** Material-specific temperature curves (continuous interpolation)
- **Attenuation:** Temperature-affected attenuation coefficients
- **Thermal gradients:** Deferred (would show interesting refraction, but complex)

#### 3.6 Transducer Modeling
- **Frequency:** Preset standard values (2.25, 5, 10 MHz) + custom input
- **Bandwidth:** Damped oscillator model with center frequency and bandwidth
  - Shows realistic pulse shapes, ringdown, dead zone
- **Dead zone:** Calculate from bandwidth, user-adjustable with empirical probe data
- **Beam pattern:** Finite aperture with divergence angle (sin θ = 1.22λ/D)
- **Angle beams:** Full wedge material modeling (perspex/plastic)
  - Wedge library with standard angles (45°, 60°, 70°) plus custom wedge designer
  - Shows refraction through wedge material

### 4. GEOMETRY SYSTEM

#### 4.1 Geometry Construction
- **Primitives:** Parametric rectangles, circles, angled blocks
- **Editing:** Vertex editing - drag vertices to create arbitrary convex polygons
- **Curved surfaces:** Circular arc support (critical for pipe inspection)
- **Input method:** Draggable with snap-to-grid (optional numeric input)
- **Visual feedback:** Ghost preview while dragging, simulation runs on mouse release

#### 4.2 Materials Database
- **Preset materials:** Steel, aluminum, titanium, water, plastics with standard properties
- **Custom materials:** User-definable velocity, density, attenuation, acoustic impedance
- **Properties:**
  - Longitudinal velocity (cL)
  - Shear velocity (cS)
  - Density (ρ)
  - Attenuation coefficient(s)
  - Temperature-velocity curves
  - Acoustic impedance (calculated: Z = ρc)

#### 4.3 Units System
- **Base units:** Metric (mm)
- **Display conversion:** JSON config mapping to other units (e.g., inches)
- **Conversion layer:** Display-level only (calculations in base units)

#### 4.4 Validation
- **Mode:** Warnings only (recommended), with toggle to disable
- **Checks:** Impossible wedge angles, negative dimensions, unphysical parameters
- **Philosophy:** Highlight issues but allow simulation (educational exploration)

### 5. USER INTERFACE

#### 5.1 Layout
- **Main geometric view:** 2D simulation workspace
- **A-scan display:** Separate panel/module (independent learning environment)
- **Properties/controls:** Parameter editing panels
- **Panel system:** Resizable dividers between sections
- **Equation panel:** Collapsible panel showing formulas + substituted values for selected rays/interfaces

#### 5.2 Theming
- **Themes:** 3-4 presets (light, dark, high contrast, colorblind-friendly)
- **Color system:** Architecturally designed for easy theme modification
- **Wave colors:** Distinguishable L vs S waves (implementation judgment)

#### 5.3 Visualization Controls
- **Time control:** Timeline scrubber (drag to any moment, optional play)
- **View coupling:** A-scan ↔ geometry linked with highlighting
  - Hover A-scan peak → highlight reflection path in geometry
  - Hover geometry feature → highlight corresponding echo
- **Display bounds:** Clip at workspace edges (user-controlled scale)
- **Wavefront style:** Discrete ray markers

#### 5.4 Simulation Control
- **Trigger:** Manual "Run Simulation" button (recommended)
  - Toggle for auto-run with lag warning on hover
- **Fidelity settings:** Simulation Settings menu
  - Allows toggling between simple/complex physics models
  - Progressive complexity (implement simple baseline, extend later)

#### 5.5 Measurement Tools
- **Ruler/distance:** Click two points to measure
- **Grid/reference:** Visible grid with unit labels
- **Angle measurement:** (Future consideration)

#### 5.6 Help System
- **Tooltips:** Hover over parameters shows brief explanation and formulas (e.g., Z=ρc)
- **Advanced:** Victoria 3-style nested tooltips showing calculation chains
  - Deferred to post-MVP (but consider architectural support)
  - Hover values in tooltip to see their component calculations
- **In-app help:** Contextual explanations without cluttering UI

#### 5.7 Undo System
- **Implementation:** Basic undo/redo with limited history
- **Scope:** Geometry edits and parameter changes

### 6. A-SCAN MODULE

#### 6.1 Display Modes
- **RF view:** Full oscillating waveform at probe frequency
- **Rectified envelope:** Standard flaw detector view
- **Toggle:** Switch between RF and envelope (matches real equipment)

#### 6.2 Controls
- **Range:** Maximum time/distance displayed
- **Delay:** Start time offset
- **Gain:** Overall amplitude multiplier (shows saturation/missed echoes)
- **TCG/DAC:** User-defined correction curves
  - Click to place DAC points from calibration blocks
  - Matches real UT workflow

#### 6.3 Echo Handling
- **Multiple echoes:** Color-coded by source with transparency
- **Overlapping echoes:** Artificial separation for pedagogy
- **Dead zone:** Mask echoes overlapping initial pulse (based on bandwidth)

#### 6.4 Calibration Mode
- **Velocity calibration:** Input known distance + TOF → calculate velocity
- **DAC points:** User-defined amplitude correction points

### 7. WORKFLOW FEATURES

#### 7.1 Example Library
- **Included scenarios:**
  - Basic contact testing
  - Angle beam weld inspection
  - Thickness gauging
  - Bolt hole inspection (circular geometry)
- **Format:** JSON files loaded as presets

#### 7.2 Save/Load
- **Format:** JSON export/import (human-readable)
- **Contents:** Complete simulation setup (geometry, materials, transducer, settings)
- **Additional exports:**
  - PNG screenshots of current view
  - A-scan waveform as byte array with time sequence

#### 7.3 Distribution
- **Format:** Pre-built binaries (GitHub releases)
- **Platforms:** .exe for Windows, Linux binary if practical
- **Documentation:** Build instructions for source compilation

### 8. IMPLEMENTATION PHASES

#### 8.1 MVP - Minimum Viable Product
**Core features for initial useful tool:**
- Single transducer placement (draggable)
- Basic geometry construction (rectangles, drag vertices)
- Longitudinal waves only initially
- Simple ray tracing with multi-bounce
- Basic A-scan display (envelope mode)
- Material presets (steel, aluminum, water)
- Light/dark theme toggle
- Timeline scrubber
- JSON save/load
- Example scenario (thickness gauge, simple weld)

**Physics:**
- Geometric spreading
- Reflection coefficients (normal incidence)
- Simple dB/mm attenuation
- Time of flight calculations

#### 8.2 Phase 2 - Complete Wave Physics
- Shear waves and mode conversion
- Oblique incidence (unified formulation)
- Angle beam transducers with wedges
- Frequency-dependent attenuation
- Bandwidth/dead zone modeling
- RF view toggle
- Equation display panel

#### 8.3 Phase 3 - Advanced Features
- Temperature effects
- Near/far field zones
- Curved geometry (circular arcs)
- Surface roughness parameter
- TCG/DAC curves
- Calibration mode
- Measurement tools
- Additional themes
- Enhanced example library

#### 8.4 Future Considerations
- Victoria 3-style nested tooltips
- B-scan capability (using A-scan time sequence exports)
- Immersion testing mode
- Surface wave modeling
- Edge diffraction physics
- Extended material models
- Advanced probe modeling (phased array concepts)

### 9. ARCHITECTURAL CONSIDERATIONS

#### 9.1 Extensibility
- **Simulation settings:** Menu-driven toggles for physics complexity
- **Fidelity levels:** Support for extending models (point source → finite aperture → near/far field)
- **Module structure:** A-scan as independent module
- **Calculation provenance:** Track calculation chains (future nested tooltips)

#### 9.2 Performance
- **Real-time rendering:** Acceptable lag for typical scenarios
- **Complexity management:** Amplitude cutoff, Range+Delay limits
- **Optimization:** Not over-prioritized (modern PC adequate)

#### 9.3 Code Quality
- **Learning project:** C++ learning opportunity with mentor access
- **Clarity:** Prefer clear code over premature optimization
- **Physics accuracy:** Reference Krautkramer and standard UT textbooks

### 10. VALIDATION & TESTING

#### 10.1 Physics Validation
- **Empirical data:** Test against real probe measurements from work
- **Textbook scenarios:** Verify against Krautkramer examples
- **Edge cases:** Normal incidence, critical angles, mode conversion

#### 10.2 Usability Testing
- **Workflow match:** Compare to real UT equipment operation
- **Pedagogical value:** Test learning curve for UT concepts

### 11. OPEN QUESTIONS & DESIGN DECISIONS

#### 11.1 Deferred Decisions
- Beam visualization (ray paths vs. intensity field contours) - evaluate during implementation
- Specific color schemes - informed judgment during theme implementation
- Performance optimizations - profile before optimizing

#### 11.2 Scope Boundaries
- **In scope:** Contact testing, angle beams, basic material physics
- **Out of scope (v1):** Immersion testing, phased arrays, TOFD, full surface wave modeling
- **Future potential:** B-scan, advanced visualization, automation/scripting

### 12. SUCCESS CRITERIA

#### 12.1 MVP Success
- Can accurately model basic contact testing scenario
- Time of flight matches hand calculations
- Reflection coefficients match textbook values
- Usable for visualizing concepts currently drawn manually
- Clean, pedagogically clear visualizations

#### 12.2 Long-term Success
- Used as teaching tool for UT concepts
- Reduces manual drawing/calculation time
- Enables "what-if" exploration of UT scenarios
- Matches or exceeds hand-calculation accuracy