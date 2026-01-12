#pragma once

namespace BeamCast {

// Physics constants for ultrasonic testing simulation
// References: Krautkramer "Ultrasonic Testing of Materials" (4th Edition)

namespace Physics {
    // Beam Spreading (Rayleigh Criterion)
    // Reference: Krautkramer Chapter 2, Section 2.2.3
    constexpr double BEAM_DIVERGENCE_COEFFICIENT = 1.22;
    // For circular aperture: sin(θ) ≈ 1.22λ/D
    // This is the first minimum of the Airy diffraction pattern

    // Beam Pattern Shaping
    constexpr double GAUSSIAN_DECAY_FACTOR = 4.0;
    // Controls Gaussian amplitude rolloff from beam center
    // k=2: ~60% amplitude at edges, k=3: ~50%, k=4: ~37%
    // Higher values = more focused beam with sharper dropoff
    // --
    // andr-be modified: I've changed the decay factor to 4.0 as part of the 'feel' changes 
}

namespace CouplingConstants {
    // Acoustic Coupling Efficiency Values
    // Reference: Krautkramer Chapter 4, Section 4.2

    constexpr double GOOD_CONTACT_EFFICIENCY = 0.95;
    // 5% loss typical for gel or oil couplant
    // Accounts for impedance mismatch and couplant layer

    constexpr double AIR_GAP_EFFICIENCY = 0.001;
    // 0.1% typical for air coupling
    // Practically no transmission due to large impedance mismatch
    // Z_air ≈ 400 kg/(m²s) vs Z_steel ≈ 45×10⁶ kg/(m²s)

    // Future expansion for Phase 3:
    // constexpr double WATER_IMMERSION_EFFICIENCY = 0.85;  // Immersion testing
    // constexpr double ROUGH_SURFACE_FACTOR = 0.7;         // Surface roughness effect
}

namespace TransducerConstants {
    // Transducer Electromechanical Efficiency
    // Models combined transmit + receive sensitivity before gain stage

    constexpr double PREAMPLIFIER_SENSITIVITY = 0.02;
    // 25% round-trip efficiency (0.5 transmit × 0.5 receive)
    // Represents piezoelectric conversion losses both ways
    // Applied to all echoes before gain stage
    // Real UT: Transducers have ~50-70% efficiency each direction
    // Combined: ~25-50% round-trip before amplification
    // --
    // andr-be modified: I've scaled this down to 0.02 because it feels better
}

namespace AttenuationConstants {
    // Attenuation Coefficients (dB/mm at reference frequency)
    // Reference: Krautkramer Chapter 3, Section 3.3

    constexpr double AIR_ATTENUATION_DB_PER_MM = 100.0;
    // Approximate for MHz ultrasound in air
    // Extremely high - effectively blocks propagation
    // Reality: frequency-dependent, but 100 dB/mm models "no propagation"

    constexpr double STEEL_ATTENUATION_DB_PER_MM = 0.01;
    // Typical for steel at 5 MHz
    // Low loss material - good for UT

    constexpr double ALUMINUM_ATTENUATION_DB_PER_MM = 0.02;
    // Typical for aluminum at 5 MHz
    // Slightly higher loss than steel due to grain structure
}

namespace RayTracingDefaults {
    // Default simulation parameters for ray tracing

    constexpr double DEFAULT_AMPLITUDE_THRESHOLD = 0.01;
    // 1% amplitude cutoff - typical detection limit for real flaw detectors

    constexpr double DEFAULT_MAX_TIME_OF_FLIGHT_US = 200.0;
    // 200μs default A-scan range
    // Covers ~600mm in steel (two-way)

    constexpr int DEFAULT_MAX_BOUNCES = 50;
    // Prevent infinite loops in complex geometries
    // Time window typically limits rays before this is reached

    constexpr double SURFACE_OFFSET_MM = 0.01;
    // Small offset to prevent self-intersection at interfaces
    // 0.01mm << typical wavelength (0.3-3mm at 1-10 MHz)
}

namespace RenderingConstants {
    // Visual display parameters (not physics)

    constexpr double BEAM_CONE_VISUAL_LENGTH_MM = 25.0;
    // Display length for beam spread indicator overlay

    constexpr double TRANSDUCER_DIRECTION_INDICATOR_LENGTH_MM = 15.0;
    // Length of direction arrow from transducer center

    constexpr double TRANSDUCER_CLICK_MARGIN_MM = 5.0;
    // Extra clickable area around transducer for easier interaction

    // Ray Opacity Scaling (quadratic for emphasis)
    constexpr uint8_t MIN_RAY_ALPHA = 1;   // 8% min opacity (very faint)
    constexpr uint8_t MAX_RAY_ALPHA = 20;  // 78% max opacity (bright)
    // Note: Alpha scales as amplitude² to make weak rays very faint
    // --
    // andrbe comment: I can't really see that the alpha works at all; changing it doesn't appear to change much at all! 
}

namespace AScanDisplay {
    // A-scan visualization parameters

    constexpr double MAX_AMPLITUDE_DISPLAY_FRACTION = 0.99;
    // Leave 1% margin at top of A-scan display
    // Prevents echoes from touching the upper border

    constexpr double REFLECTION_DOT_TOLERANCE_DEG = 8.0;
    // Minimum angle change to detect a reflection echo
    // cos(8°) ≈ 0.99 - rays changing direction by >8° are reflections

    // Electronic noise parameters
    constexpr double BASE_NOISE_AMPLITUDE = 0.02;
    // Baseline noise floor at 0 dB gain (5% amplitude)
    // Represents thermal/electronic noise in amplifier circuits
    // This is much higher than real equipment to make noise visible at low gains

    constexpr double NOISE_GAIN_SCALING = 0.5;
    // Noise scaling exponent for gain dependency
    // noiseAmplitude = baseNoise × (gainLinear ^ NOISE_GAIN_SCALING)
    // 0.5 = square root scaling (noise power scales linearly with gain)
}

namespace UIControls {
    // User interface control increments and limits

    constexpr double BEAM_SPREAD_INCREMENT_DEG = 5.0;
    constexpr double BEAM_SPREAD_MIN_DEG = 5.0;
    constexpr double BEAM_SPREAD_MAX_DEG = 90.0;

    constexpr int RAY_COUNT_INCREMENT = 4;
    constexpr int RAY_COUNT_MIN = 4;
    constexpr int RAY_COUNT_MAX = 512;

    constexpr double AMPLITUDE_THRESHOLD_INCREMENT = 0.01;  // 1%
    constexpr double AMPLITUDE_THRESHOLD_MIN = 0.0;
    constexpr double AMPLITUDE_THRESHOLD_MAX = 1.0;

    constexpr double ASCAN_RANGE_INCREMENT_US = 5.0;
    constexpr double ASCAN_RANGE_MIN_US = 5.0;
    constexpr double ASCAN_RANGE_MAX_US = 500.0;

    constexpr double ASCAN_DELAY_INCREMENT_US = 1.0;
    constexpr double ASCAN_DELAY_MIN_US = 0.0;
    constexpr double ASCAN_DELAY_MAX_US = 100.0;

    constexpr double GEOMETRY_ROTATION_INCREMENT_DEG = 15.0;
}

} // namespace BeamCast
