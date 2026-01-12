#pragma once

#include "MathTypes.h"
#include "Material.h"
#include "PhysicsConstants.h"
#include <vector>

namespace BeamCast {

// Transducer represents an ultrasonic probe
class Transducer {
public:
    Vec2 position;           // Center position (mm)
    double angle;            // Aperture orientation in radians (0 = horizontal, perpendicular to +Y)
    double beamAngle;        // Beam steering angle relative to aperture normal (radians)
                             // 0 = perpendicular to aperture, positive = clockwise from normal
    double frequency;        // Center frequency (MHz)
    double bandwidth;        // Fractional bandwidth (e.g., 0.5 for 50%)
    double elementDiameter;  // Probe element diameter (mm)

    // Angle beam probe wedge properties
    Material wedgeMaterial;  // Wedge material (typically acrylic/perspex)
    double wedgeAngle;       // Physical wedge angle in radians (angle in wedge material)
                             // This is the "nominal" probe angle (e.g., 45°, 60°, 70°)
                             // The actual refracted angle in test material depends on Snell's law

    // Pulse parameters
    double pulseAmplitude;   // Initial amplitude (0-1)

    // Beam model fidelity (for future extension)
    enum BeamModel {
        POINT_SOURCE,        // Simple point source
        FINITE_APERTURE,     // Includes beam spreading
        NEAR_FAR_FIELD       // Full near/far field modeling
    } beamModel;

    Transducer()
        : position(0, -60)   // Default below geometry
        , angle(0.0)         // Aperture horizontal
        , beamAngle(0.0)     // Beam perpendicular to aperture (pointing up)
        , frequency(5.0)     // 5 MHz
        , bandwidth(0.5)     // 50% bandwidth
        , elementDiameter(12.0)  // 12mm diameter
        , wedgeMaterial(Materials::Perspex())  // Standard acrylic wedge
        , wedgeAngle(0.0)    // 0° = straight beam probe (no wedge)
        , pulseAmplitude(1.0)
        , beamModel(POINT_SOURCE)
    {}

    Transducer(const Vec2& pos, double angleRad, double freq = 5.0)
        : position(pos)
        , angle(angleRad)
        , beamAngle(0.0)     // Default: beam perpendicular to aperture
        , frequency(freq)
        , bandwidth(0.5)
        , elementDiameter(12.0)
        , wedgeMaterial(Materials::Perspex())
        , wedgeAngle(0.0)
        , pulseAmplitude(1.0)
        , beamModel(POINT_SOURCE)
    {}

    // Get the aperture normal direction (perpendicular to aperture face)
    // This is the direction perpendicular to the line segment
    Vec2 getApertureNormal() const {
        // Angle 0 = horizontal aperture, normal points up (+Y)
        return Vec2(0, 1).rotated(angle);
    }

    // Get the beam direction (may be steered relative to aperture normal)
    // This is the direction the ultrasound beam propagates
    Vec2 getDirection() const {
        Vec2 normal = getApertureNormal();
        // Positive beamAngle rotates clockwise from normal (standard NDT convention)
        return normal.rotated(beamAngle);
    }

    // Get the transducer element as a line segment (aperture face)
    // Returns the two endpoints of the line segment perpendicular to aperture normal
    // The aperture orientation is fixed (stays aligned with surface)
    void getApertureSegment(Vec2& p1, Vec2& p2) const {
        Vec2 normal = getApertureNormal();  // Aperture normal (perpendicular to face)
        Vec2 perpendicular = normal.perpendicular();  // Direction along the aperture
        double halfWidth = elementDiameter / 2.0;
        p1 = position - perpendicular * halfWidth;
        p2 = position + perpendicular * halfWidth;
    }

    // Get left endpoint of aperture (looking along aperture normal)
    Vec2 getApertureLeft() const {
        Vec2 normal = getApertureNormal();
        Vec2 perpendicular = normal.perpendicular();
        return position - perpendicular * (elementDiameter / 2.0);
    }

    // Get right endpoint of aperture (looking along aperture normal)
    Vec2 getApertureRight() const {
        Vec2 normal = getApertureNormal();
        Vec2 perpendicular = normal.perpendicular();
        return position + perpendicular * (elementDiameter / 2.0);
    }

    // Get wavelength in a given material (λ = c/f)
    double getWavelength(const Material& mat, bool shear = false) const {
        double velocity = shear ? mat.velocityShear : mat.velocityLongitudinal;
        // Velocity is in mm/μs, frequency in MHz
        // λ = v/f = (mm/μs) / MHz = (mm/μs) / (1/μs) = mm
        return velocity / frequency;
    }

    // Calculate beam divergence half-angle (radians)
    // Using sin(θ) ≈ 1.22 λ/D for far field
    double getBeamDivergence(const Material& mat, bool shear = false) const {
        if (beamModel == POINT_SOURCE) {
            return Math::HALF_PI; // Point source radiates everywhere
        }

        double wavelength = getWavelength(mat, shear);
        double sinTheta = Physics::BEAM_DIVERGENCE_COEFFICIENT * wavelength / elementDiameter;

        // Clamp to prevent domain error
        if (sinTheta >= 1.0) return Math::HALF_PI;
        return std::asin(sinTheta);
    }

    // Calculate near field length N = D²f/(4c)
    // Returns distance in mm
    double getNearFieldLength(const Material& mat, bool shear = false) const {
        if (beamModel == POINT_SOURCE) return 0.0;

        double velocity = shear ? mat.velocityShear : mat.velocityLongitudinal;
        // N = D²/(4λ) = D²f/(4c)
        // D in mm, f in MHz, c in mm/μs
        double wavelength = getWavelength(mat, shear);
        return (elementDiameter * elementDiameter) / (4.0 * wavelength);
    }

    // Calculate pulse duration (μs) based on bandwidth
    // Δt ≈ 1/Δf, where Δf = f × bandwidth
    double getPulseDuration() const {
        return 1.0 / (frequency * bandwidth);
    }

    // Calculate dead zone distance in a material
    double getDeadZone(const Material& mat, bool shear = false) const {
        double velocity = shear ? mat.velocityShear : mat.velocityLongitudinal;
        double pulseDuration = getPulseDuration();
        // Distance = velocity × time (half the pulse duration for round trip)
        return velocity * pulseDuration / 2.0;
    }

    // Calculate refracted beam angle in test material using Snell's law
    // Returns the refracted angle in radians, or -1.0 if total internal reflection occurs
    //
    // Snell's law: sin(θ₁)/c₁ = sin(θ₂)/c₂
    // Where:
    //   θ₁ = wedgeAngle (angle in wedge material)
    //   c₁ = wedge material velocity
    //   θ₂ = refracted angle in test material (what we calculate)
    //   c₂ = test material velocity
    //
    // Example: 45° probe in steel
    //   - Wedge: Perspex @ 2.73 mm/μs, θ₁ ≈ 62° (calculated to give 45° in steel)
    //   - Steel: 5.9 mm/μs
    //   - sin(θ₂) = sin(62°) × (5.9/2.73) ≈ 0.707 → θ₂ = 45°
    double calculateRefractedAngle(const Material& testMaterial, bool shear = false) const {
        // If wedgeAngle is 0, it's a straight beam probe (no refraction needed)
        if (std::abs(wedgeAngle) < 1e-6) {
            return 0.0;
        }

        // Get velocities
        double wedgeVelocity = wedgeMaterial.velocityLongitudinal;  // Wave in wedge is always longitudinal
        double testVelocity = shear ? testMaterial.velocityShear : testMaterial.velocityLongitudinal;

        // Snell's law: sin(θ₂) = sin(θ₁) × (c₂/c₁)
        double sinTheta1 = std::sin(wedgeAngle);
        double sinTheta2 = sinTheta1 * (testVelocity / wedgeVelocity);

        // Check for total internal reflection
        if (std::abs(sinTheta2) > 1.0) {
            return -1.0;  // Total internal reflection - angle not possible
        }

        return std::asin(sinTheta2);
    }

    // Set probe to standard angle beam configuration for given target angle in steel
    // This calculates the required wedge angle to achieve the target angle in steel
    // Standard angles: 45°, 60°, 70° (for steel)
    void setAngleBeamProbe(double targetAngleDegrees) {
        // Target angle in steel (reference material)
        Material steel = Materials::Steel();
        double targetAngleRad = Math::toRadians(targetAngleDegrees);

        // Calculate required wedge angle using inverse Snell's law
        // sin(θ_wedge) = sin(θ_steel) × (c_wedge / c_steel)
        double sinTargetAngle = std::sin(targetAngleRad);
        double wedgeVelocity = wedgeMaterial.velocityLongitudinal;
        double steelVelocity = steel.velocityLongitudinal;

        double sinWedgeAngle = sinTargetAngle * (wedgeVelocity / steelVelocity);

        // Check if achievable
        if (std::abs(sinWedgeAngle) > 1.0) {
            // Can't achieve this angle with this wedge material
            wedgeAngle = 0.0;
            return;
        }

        wedgeAngle = std::asin(sinWedgeAngle);
    }

    // Ray with initial amplitude and origin based on beam pattern
    struct BeamRay {
        Vec2 origin;       // Starting point on the aperture
        Vec2 direction;
        double amplitude;  // Initial amplitude based on beam directivity (0-1)

        BeamRay(const Vec2& orig, const Vec2& dir, double amp)
            : origin(orig), direction(dir), amplitude(amp) {}
    };

    // Generate initial rays for ray tracing with beam directivity pattern
    // Rays originate from distributed points along the aperture line segment
    // beamSpreadDegrees: total angular width (e.g., 20° means ±10° from center)
    // Returns rays with amplitude scaled by Gaussian beam pattern
    //
    // For realistic beam modeling:
    // - Rays originate from points distributed along the aperture
    // - Each ray has a direction within the beam cone
    // - Amplitude is based on Gaussian beam pattern
    std::vector<BeamRay> generateBeamRays(int numRays, double beamSpreadDegrees) const {
        std::vector<BeamRay> beamRays;
        beamRays.reserve(numRays);

        Vec2 mainDirection = getDirection();
        double halfSpreadRadians = Math::toRadians(beamSpreadDegrees / 2.0);

        // Get aperture endpoints
        Vec2 apertureLeft, apertureRight;
        getApertureSegment(apertureLeft, apertureRight);

        // Distribute rays across both:
        // 1. Aperture positions (spatial)
        // 2. Angular directions (beam cone)
        //
        // Strategy: Use sqrt(numRays) points along aperture, sqrt(numRays) angles
        // This gives good coverage in both dimensions
        int raysPerAperturePoint = std::max(1, (int)std::sqrt((double)numRays));
        int aperturePoints = std::max(1, numRays / raysPerAperturePoint);

        for (int ap = 0; ap < aperturePoints; ap++) {
            // Position along aperture (t = 0 is left, t = 1 is right)
            double apertureT = (aperturePoints == 1) ? 0.5 : (double)ap / (aperturePoints - 1);
            Vec2 rayOrigin = apertureLeft + (apertureRight - apertureLeft) * apertureT;

            for (int ar = 0; ar < raysPerAperturePoint; ar++) {
                // Angular distribution within beam cone
                double angleT = (raysPerAperturePoint == 1) ? 0.5 : (double)ar / (raysPerAperturePoint - 1);
                double angle = -halfSpreadRadians + 2.0 * halfSpreadRadians * angleT;

                // Calculate beam directivity amplitude using Gaussian pattern
                // Peak at center (angle=0), drops off toward edges
                double normalizedAngle = angle / halfSpreadRadians;  // -1 to +1
                double amplitude = std::exp(-Physics::GAUSSIAN_DECAY_FACTOR * normalizedAngle * normalizedAngle);

                Vec2 dir = mainDirection.rotated(angle);
                beamRays.emplace_back(rayOrigin, dir, amplitude);
            }
        }

        return beamRays;
    }

    // Legacy method for backward compatibility
    std::vector<Vec2> generateRayDirections(int numRays, double beamSpreadDegrees) const {
        std::vector<Vec2> directions;
        directions.reserve(numRays);

        Vec2 mainDirection = getDirection();
        double halfSpreadRadians = Math::toRadians(beamSpreadDegrees / 2.0);

        for (int i = 0; i < numRays; i++) {
            double t = (numRays == 1) ? 0.5 : (double)i / (numRays - 1);
            double angle = -halfSpreadRadians + 2.0 * halfSpreadRadians * t;
            Vec2 dir = mainDirection.rotated(angle);
            directions.push_back(dir);
        }

        return directions;
    }
};

} // namespace BeamCast
