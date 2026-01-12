#pragma once

#include "MathTypes.h"
#include "Geometry.h"
#include "Material.h"
#include "Transducer.h"
#include <vector>
#include <memory>
#include <limits>
#include <iostream>

namespace BeamCast {

// Represents a traced ray path segment
struct RaySegment {
    Vec2 start;
    Vec2 end;
    double amplitude;      // Current amplitude (0-1)
    double timeOfFlight;   // Time to reach this point (μs)
    Ray::WaveType waveType;
    Material material;     // Material this segment travels through

    RaySegment(const Vec2& s, const Vec2& e, double amp, double tof, Ray::WaveType type, const Material& mat)
        : start(s), end(e), amplitude(amp), timeOfFlight(tof), waveType(type), material(mat)
    {}
};

// Result of ray-geometry intersection
struct RayHit {
    bool hit;
    Vec2 point;            // Intersection point
    Vec2 normal;           // Surface normal at intersection
    double distance;       // Distance from ray origin
    const GeometryObject* object; // Hit object
    int edgeIndex;         // Which edge was hit

    RayHit() : hit(false), point(0,0), normal(0,0), distance(std::numeric_limits<double>::infinity()), object(nullptr), edgeIndex(-1) {}
};

class RayTracer {
public:
    std::vector<RaySegment> tracedPaths;  // All traced ray segments for visualization
    double amplitudeThreshold;             // Stop tracing below this amplitude
    int maxBounces;                        // Maximum number of bounces
    Material couplingMedium;               // Medium between transducer and geometry (usually air or water)

    RayTracer()
        : amplitudeThreshold(0.01)  // 1% cutoff - better default for practical UT
        , maxBounces(10)
        , couplingMedium(Materials::Air())
    {}

    // Trace all rays from a transducer through geometry
    void traceFromTransducer(const Transducer& transducer,
                            const std::vector<std::unique_ptr<GeometryObject>>& geometries,
                            int numRays,
                            double beamSpreadDegrees) {
        tracedPaths.clear();

        // Determine starting medium: check if transducer is inside any geometry
        Material startingMedium = couplingMedium;  // Default: air
        bool insideGeometry = false;

        for (const auto& geom : geometries) {
            if (geom->contains(transducer.position)) {
                // Transducer is inside this geometry (snapped to surface)
                startingMedium = geom->material;
                insideGeometry = true;
                break;
            }
        }

        // Generate rays with beam directivity pattern
        std::vector<Transducer::BeamRay> beamRays = transducer.generateBeamRays(numRays, beamSpreadDegrees);

        // Trace each ray with initial amplitude from beam pattern
        for (const auto& beamRay : beamRays) {
            Ray ray(transducer.position, beamRay.direction, Ray::LONGITUDINAL);
            ray.amplitude = beamRay.amplitude;  // Set initial amplitude based on beam directivity

            // If transducer is inside geometry (snapped), apply coupling efficiency
            // For MVP: Simple model - coupling reduces initial amplitude
            if (insideGeometry) {
                constexpr double COUPLING_EFFICIENCY = 0.95;  // 95% transmission (5% loss at interface)
                ray.amplitude *= COUPLING_EFFICIENCY;
            }

            traceRay(ray, geometries, 0, startingMedium);
        }
    }

private:
    // Recursively trace a single ray with current material context
    void traceRay(const Ray& ray, const std::vector<std::unique_ptr<GeometryObject>>& geometries,
                  int bounceCount, const Material& currentMedium) {
        if (ray.amplitude < amplitudeThreshold || bounceCount >= maxBounces) {
            return; // Ray too weak or too many bounces
        }

        // Find nearest intersection
        RayHit hit = findNearestIntersection(ray, geometries);

        if (!hit.hit) {
            // Ray goes to infinity - draw a long segment with attenuation
            constexpr double MAX_RAY_DISTANCE_MM = 200.0;
            Vec2 endPoint = ray.origin + ray.direction * MAX_RAY_DISTANCE_MM;

            // Apply attenuation over distance in current medium
            double attenuation = std::exp(-currentMedium.attenuationCoeff * MAX_RAY_DISTANCE_MM);
            double endAmplitude = ray.amplitude * attenuation;

            tracedPaths.emplace_back(ray.origin, endPoint, endAmplitude, 0.0, ray.waveType, currentMedium);
            return;
        }

        // Ray travels through current medium until it hits
        double distToHit = hit.distance;
        double velocity = (ray.waveType == Ray::LONGITUDINAL) ?
            currentMedium.velocityLongitudinal :
            currentMedium.velocityShear;

        double timeOfFlight = distToHit / velocity;

        // Apply attenuation over distance in current medium
        double attenuation = std::exp(-currentMedium.attenuationCoeff * distToHit);
        double newAmplitude = ray.amplitude * attenuation;

        // Add this segment to traced paths (ray travels through currentMedium)
        tracedPaths.emplace_back(ray.origin, hit.point, newAmplitude, timeOfFlight, ray.waveType, currentMedium);

        // Interface physics: reflection and transmission
        // Determine incident angle (angle between ray and surface normal)
        double cosIncident = -(ray.direction.dot(hit.normal));

        // Determine if ray is entering or exiting the object
        // If currentMedium matches the hit object's material, we're INSIDE and exiting
        // If they don't match, we're OUTSIDE and entering
        bool isExiting = (currentMedium.name == hit.object->material.name);

        // Surface normal always points away from incident medium
        Vec2 surfaceNormal = hit.normal;
        Material incidentMedium = currentMedium;
        Material transmittedMedium = hit.object->material;

        if (isExiting) {
            // Ray is exiting the object - flip normal to point into transmitted medium
            surfaceNormal = surfaceNormal * -1.0;
            cosIncident = -cosIncident;
            incidentMedium = hit.object->material;
            transmittedMedium = couplingMedium;
        }

        double incidentAngle = std::acos(Math::clamp(cosIncident, 0.0, 1.0));

        // Calculate reflection and transmission coefficients
        // For MVP: Use pressure reflection coefficient (simplified but physically motivated)
        // Future: Full Fresnel equations with mode conversion
        double Z1 = (ray.waveType == Ray::LONGITUDINAL) ?
            incidentMedium.getAcousticImpedance() : incidentMedium.getShearImpedance();
        double Z2 = (ray.waveType == Ray::LONGITUDINAL) ?
            transmittedMedium.getAcousticImpedance() : transmittedMedium.getShearImpedance();

        // Pressure reflection coefficient: R = (Z2 - Z1) / (Z2 + Z1)
        // Intensity reflection coefficient: R²
        double reflectionCoeff = (Z2 - Z1) / (Z2 + Z1);
        double reflectionIntensity = reflectionCoeff * reflectionCoeff;

        // Transmission coefficient: T = 1 - R² (energy conservation)
        double transmissionIntensity = 1.0 - reflectionIntensity;

        // REFLECTED RAY
        constexpr double MIN_RAY_COEFFICIENT = 0.01;  // 1% threshold
        constexpr double SURFACE_OFFSET_MM = 0.01;     // Small offset to avoid self-intersection

        if (reflectionIntensity > MIN_RAY_COEFFICIENT) {
            // Reflected direction: r = d - 2(d·n)n
            // IMPORTANT: Use original hit.normal for reflection, NOT flipped surfaceNormal
            // Reflection is a geometric operation that doesn't depend on which side we're on
            Vec2 reflectedDir = ray.direction - hit.normal * (2.0 * ray.direction.dot(hit.normal));

            // Offset reflected ray back into incident medium
            // If exiting, offset opposite to surfaceNormal (back into material)
            // If entering, offset along surfaceNormal (away from surface)
            Vec2 offsetDir = isExiting ? (surfaceNormal * -1.0) : surfaceNormal;
            Ray reflectedRay(hit.point + offsetDir * SURFACE_OFFSET_MM, reflectedDir, ray.waveType);
            reflectedRay.amplitude = newAmplitude * std::sqrt(reflectionIntensity);  // Amplitude scales as sqrt(intensity)
            reflectedRay.distance = ray.distance + distToHit;

            // Reflected ray stays in incident medium
            traceRay(reflectedRay, geometries, bounceCount + 1, incidentMedium);
        }

        // TRANSMITTED RAY (with Snell's law refraction)
        if (transmissionIntensity > MIN_RAY_COEFFICIENT) {
            // Snell's law: n1·sin(θ1) = n2·sin(θ2)
            // For acoustics: n = 1/c (refractive index inversely proportional to velocity)
            double v1 = (ray.waveType == Ray::LONGITUDINAL) ?
                incidentMedium.velocityLongitudinal : incidentMedium.velocityShear;
            double v2 = (ray.waveType == Ray::LONGITUDINAL) ?
                transmittedMedium.velocityLongitudinal : transmittedMedium.velocityShear;

            // Handle liquids (no shear waves)
            if (ray.waveType == Ray::SHEAR && (v2 < 1e-6 || v1 < 1e-6)) {
                // Shear wave can't propagate in liquid - no transmission
                return;
            }

            double sinIncident = std::sin(incidentAngle);
            double sinTransmitted = (v2 / v1) * sinIncident;

            // Check for total internal reflection (critical angle exceeded)
            if (std::abs(sinTransmitted) > 1.0) {
                // Total internal reflection - no transmitted ray
                return;
            }

            double transmittedAngle = std::asin(Math::clamp(sinTransmitted, -1.0, 1.0));
            double cosTransmitted = std::cos(transmittedAngle);

            // Calculate transmitted direction using Snell's law
            // Transmitted ray = sin(θt)·tangent + cos(θt)·(-normal)
            Vec2 tangent = (ray.direction - surfaceNormal * cosIncident).normalized();
            Vec2 transmittedDir = tangent * sinTransmitted - surfaceNormal * cosTransmitted;
            transmittedDir = transmittedDir.normalized();

            Ray transmittedRay(hit.point - surfaceNormal * SURFACE_OFFSET_MM, transmittedDir, ray.waveType);
            transmittedRay.amplitude = newAmplitude * std::sqrt(transmissionIntensity);
            transmittedRay.distance = ray.distance + distToHit;

            // Transmitted ray enters new medium
            traceRay(transmittedRay, geometries, bounceCount + 1, transmittedMedium);
        }

        // TODO Phase 2: Mode conversion (L→S, S→L at interfaces)
        // This requires calculating converted ray angles and amplitudes
        // Reference: Krautkramer Chapter 2, Section 2.3
    }

    // Find the nearest intersection of a ray with any geometry
    RayHit findNearestIntersection(const Ray& ray, const std::vector<std::unique_ptr<GeometryObject>>& geometries) const {
        RayHit nearestHit;

        for (const auto& geom : geometries) {
            auto vertices = geom->getVertices();

            // Check intersection with each edge
            for (size_t i = 0; i < vertices.size(); i++) {
                Vec2 v1 = vertices[i];
                Vec2 v2 = vertices[(i + 1) % vertices.size()];

                RayHit hit = rayLineSegmentIntersection(ray, v1, v2, geom.get(), (int)i);

                if (hit.hit && hit.distance < nearestHit.distance) {
                    nearestHit = hit;
                }
            }
        }

        return nearestHit;
    }

    // Test ray-line segment intersection
    RayHit rayLineSegmentIntersection(const Ray& ray, const Vec2& p1, const Vec2& p2,
                                      const GeometryObject* obj, int edgeIndex) const {
        RayHit result;

        // Ray: P = origin + t * direction  (t >= 0)
        // Line: Q = p1 + s * (p2 - p1)     (0 <= s <= 1)
        // Solve: origin + t * dir = p1 + s * (p2 - p1)

        Vec2 edgeDir = p2 - p1;
        Vec2 edgeNormal = edgeDir.perpendicular().normalized();

        // Use determinant method
        Vec2 originToP1 = p1 - ray.origin;

        double cross = ray.direction.x * edgeDir.y - ray.direction.y * edgeDir.x;

        if (std::abs(cross) < 1e-10) {
            return result; // Parallel or collinear
        }

        double t = (originToP1.x * edgeDir.y - originToP1.y * edgeDir.x) / cross;
        double s = (originToP1.x * ray.direction.y - originToP1.y * ray.direction.x) / cross;

        if (t >= 0.0 && s >= 0.0 && s <= 1.0) {
            // Valid intersection
            result.hit = true;
            result.point = ray.origin + ray.direction * t;
            result.distance = t;
            result.object = obj;
            result.edgeIndex = edgeIndex;

            // Calculate normal (pointing outward from polygon)
            // For now, assume counter-clockwise winding
            result.normal = edgeNormal;

            // Ensure normal points against ray direction
            if (result.normal.dot(ray.direction) > 0) {
                result.normal = result.normal * -1.0;
            }
        }

        return result;
    }
};

} // namespace BeamCast
