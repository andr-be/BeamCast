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
    double angle;            // Angle in radians (0 = pointing up/+Y)
    double frequency;        // Center frequency (MHz)
    double bandwidth;        // Fractional bandwidth (e.g., 0.5 for 50%)
    double elementDiameter;  // Probe element diameter (mm)

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
        , angle(0.0)         // Pointing up
        , frequency(5.0)     // 5 MHz
        , bandwidth(0.5)     // 50% bandwidth
        , elementDiameter(12.0)  // 12mm diameter
        , pulseAmplitude(1.0)
        , beamModel(POINT_SOURCE)
    {}

    Transducer(const Vec2& pos, double angleRad, double freq = 5.0)
        : position(pos)
        , angle(angleRad)
        , frequency(freq)
        , bandwidth(0.5)
        , elementDiameter(12.0)
        , pulseAmplitude(1.0)
        , beamModel(POINT_SOURCE)
    {}

    // Get the direction vector the transducer is pointing
    Vec2 getDirection() const {
        // Angle 0 points up (+Y), rotates counter-clockwise
        return Vec2(0, 1).rotated(angle);
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

    // Ray with initial amplitude based on beam pattern
    struct BeamRay {
        Vec2 direction;
        double amplitude;  // Initial amplitude based on beam directivity (0-1)

        BeamRay(const Vec2& dir, double amp) : direction(dir), amplitude(amp) {}
    };

    // Generate initial rays for ray tracing with beam directivity pattern
    // For point source, generates rays in all directions
    // For finite aperture, generates rays within beam cone
    // beamSpreadDegrees: total angular width (e.g., 20° means ±10° from center)
    // Returns rays with amplitude scaled by Gaussian beam pattern
    std::vector<BeamRay> generateBeamRays(int numRays, double beamSpreadDegrees) const {
        std::vector<BeamRay> beamRays;
        beamRays.reserve(numRays);

        Vec2 mainDirection = getDirection();
        double halfSpreadRadians = Math::toRadians(beamSpreadDegrees / 2.0);

        for (int i = 0; i < numRays; i++) {
            // Distribute rays evenly across beam spread
            double t = (numRays == 1) ? 0.5 : (double)i / (numRays - 1);
            double angle = -halfSpreadRadians + 2.0 * halfSpreadRadians * t;

            // Calculate beam directivity amplitude using Gaussian pattern
            // Peak at center (angle=0), drops off toward edges
            // Using normalized angle: 0 at center, ±1 at edges
            double normalizedAngle = angle / halfSpreadRadians;  // -1 to +1

            // Gaussian beam pattern: exp(-k * angle^2)
            double amplitude = std::exp(-Physics::GAUSSIAN_DECAY_FACTOR * normalizedAngle * normalizedAngle);

            Vec2 dir = mainDirection.rotated(angle);
            beamRays.emplace_back(dir, amplitude);
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
