#pragma once

#include <string>

namespace BeamCast {

// Material properties for ultrasonic wave propagation
struct Material {
    std::string name;

    // Wave velocities (mm/μs)
    double velocityLongitudinal;  // cL - Longitudinal (compression) wave velocity
    double velocityShear;          // cS - Shear (transverse) wave velocity

    // Material properties
    double density;                // ρ (kg/m³)
    double attenuationCoeff;       // Simple attenuation coefficient (dB/mm)

    // Temperature (°C) - affects velocity
    double temperature;

    // Surface roughness parameter (0.0 = perfectly smooth, 1.0 = very rough)
    double roughness;

    Material()
        : name("Unknown")
        , velocityLongitudinal(5.9)  // Default: steel
        , velocityShear(3.2)
        , density(7850.0)
        , attenuationCoeff(0.01)
        , temperature(20.0)
        , roughness(0.0)
    {}

    Material(const std::string& name_,
             double vL, double vS,
             double density_,
             double attenuation = 0.01)
        : name(name_)
        , velocityLongitudinal(vL)
        , velocityShear(vS)
        , density(density_)
        , attenuationCoeff(attenuation)
        , temperature(20.0)
        , roughness(0.0)
    {}

    // Acoustic impedance Z = ρ × c (for longitudinal waves)
    // Units: (kg/m³) × (mm/μs) = (kg/m³) × (1000 m/s) = kg/(m²·s) × 1000
    double getAcousticImpedance() const {
        return density * velocityLongitudinal * 1000.0; // Convert mm/μs to m/s
    }

    // Acoustic impedance for shear waves
    double getShearImpedance() const {
        return density * velocityShear * 1000.0;
    }

    // Apply temperature effect to velocity (simplified linear model for now)
    // More complex material-specific curves can be added later
    double getTemperatureAdjustedVelocityL() const {
        // Typical temperature coefficient: ~-0.1% per °C for steel
        // This is a placeholder - will be replaced with material-specific curves
        double tempCoeff = -0.001; // -0.1% per °C
        double deltaT = temperature - 20.0; // Reference temperature
        return velocityLongitudinal * (1.0 + tempCoeff * deltaT);
    }

    double getTemperatureAdjustedVelocityS() const {
        double tempCoeff = -0.001;
        double deltaT = temperature - 20.0;
        return velocityShear * (1.0 + tempCoeff * deltaT);
    }
};

// Common material presets
namespace Materials {
    inline Material Steel() {
        return Material("Steel", 5.9, 3.2, 7850.0, 0.01);
    }

    inline Material Aluminum() {
        return Material("Aluminum", 6.3, 3.1, 2700.0, 0.02);
    }

    inline Material Titanium() {
        return Material("Titanium", 6.1, 3.1, 4500.0, 0.015);
    }

    inline Material Water() {
        // Water has no shear waves (liquid)
        return Material("Water", 1.48, 0.0, 1000.0, 0.002);
    }

    inline Material Perspex() {
        // Common wedge material
        return Material("Perspex", 2.73, 1.43, 1180.0, 0.05);
    }

    inline Material Air() {
        // Air attenuation is MASSIVE - ultrasound doesn't propagate well in air
        return Material("Air", 0.343, 0.0, 1.2, 100.0);  // ~100 dB/mm kills signal fast
    }
}

} // namespace BeamCast
