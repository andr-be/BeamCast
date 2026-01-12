#pragma once

#include "MathTypes.h"
#include "RayTracer.h"
#include <vector>
#include <algorithm>

namespace BeamCast {

// Represents an echo on the A-scan
struct Echo {
    double timeOfFlight;  // Time in microseconds
    double amplitude;     // Amplitude 0-1
    Vec2 reflectionPoint; // Where the echo came from
    Ray::WaveType waveType;

    Echo(double tof, double amp, const Vec2& point, Ray::WaveType type)
        : timeOfFlight(tof), amplitude(amp), reflectionPoint(point), waveType(type) {}
};

class AScan {
public:
    std::vector<Echo> echoes;

    // Display settings
    double range;        // Maximum time displayed (μs)
    double delay;        // Start time offset (μs)
    double gain;         // Overall amplitude multiplier

    AScan()
        : range(200.0)   // 200μs default range - covers longer path lengths
        , delay(0.0)
        , gain(1.0)
    {}

    // Extract echoes from traced ray paths
    // Only records echoes where reflected rays return to transducer (pulse-echo mode)
    void generateFromRayPaths(const std::vector<RaySegment>& paths, const class Transducer& transducer) {
        echoes.clear();

        // Build ray paths by tracking continuous segments
        // Each ray path starts at transducer and may bounce multiple times
        std::vector<std::vector<const RaySegment*>> rayPaths;
        std::vector<const RaySegment*> currentPath;

        constexpr double PATH_CONTINUITY_THRESHOLD_MM = 0.1;  // Segments within 0.1mm are continuous

        for (size_t i = 0; i < paths.size(); i++) {
            const auto& segment = paths[i];

            // Check if this segment continues from previous
            bool isContinuous = false;
            if (!currentPath.empty()) {
                const RaySegment* lastSeg = currentPath.back();
                double gap = segment.start.distanceTo(lastSeg->end);
                isContinuous = (gap < PATH_CONTINUITY_THRESHOLD_MM);
            }

            if (isContinuous) {
                // Continue current path
                currentPath.push_back(&segment);
            } else {
                // Start new path
                if (!currentPath.empty()) {
                    rayPaths.push_back(currentPath);
                }
                currentPath.clear();
                currentPath.push_back(&segment);
            }
        }

        // Don't forget last path
        if (!currentPath.empty()) {
            rayPaths.push_back(currentPath);
        }

        // For each ray path, check if it returns to transducer aperture
        const Vec2& transducerPos = transducer.position;
        double transducerRadius = transducer.elementDiameter / 2.0;

        for (const auto& path : rayPaths) {
            if (path.empty()) continue;

            // Calculate cumulative TOF and find reflection points
            double pathTOF = 0.0;

            for (size_t i = 0; i < path.size(); i++) {
                const RaySegment* seg = path[i];

                // Add TOF for this segment
                double velocity = (seg->waveType == Ray::LONGITUDINAL) ?
                    seg->material.velocityLongitudinal : seg->material.velocityShear;
                double segmentLength = seg->start.distanceTo(seg->end);
                pathTOF += segmentLength / velocity;

                // Check if this is a reflection point (next segment exists and direction changes)
                if (i + 1 < path.size()) {
                    const RaySegment* nextSeg = path[i + 1];

                    // Direction change indicates reflection
                    Vec2 dir1 = (seg->end - seg->start).normalized();
                    Vec2 dir2 = (nextSeg->end - nextSeg->start).normalized();
                    double dirDot = dir1.dot(dir2);

                    constexpr double REFLECTION_DOT_THRESHOLD = 0.99;  // ~8° tolerance
                    if (dirDot < REFLECTION_DOT_THRESHOLD) {
                        // This is a reflection point - check if return path intersects transducer
                        // Extend the reflected ray (nextSeg direction) and check intersection with transducer circle

                        Vec2 reflectionPoint = seg->end;
                        Vec2 returnDir = dir2;

                        // Ray-circle intersection: check if ray from reflectionPoint in returnDir intersects transducer
                        Vec2 toTransducer = transducerPos - reflectionPoint;
                        double a = returnDir.dot(returnDir);  // Should be 1.0 for normalized
                        double b = -2.0 * returnDir.dot(toTransducer);
                        double c = toTransducer.dot(toTransducer) - transducerRadius * transducerRadius;
                        double discriminant = b * b - 4 * a * c;

                        if (discriminant >= 0 && b < 0) {  // Ray heading toward circle
                            double t = (-b - std::sqrt(discriminant)) / (2 * a);

                            if (t > 0) {  // Intersection ahead of reflection point
                                // Valid echo - calculate return path TOF
                                Vec2 intersectionPoint = reflectionPoint + returnDir * t;
                                double returnDistance = reflectionPoint.distanceTo(intersectionPoint);

                                // Use velocity of the return path segments
                                // For simplicity, use the material of the next segment
                                double returnVelocity = (nextSeg->waveType == Ray::LONGITUDINAL) ?
                                    nextSeg->material.velocityLongitudinal : nextSeg->material.velocityShear;
                                double returnTOF = returnDistance / returnVelocity;

                                double totalTOF = pathTOF + returnTOF;

                                // Calculate receive angle amplitude scaling
                                Vec2 transducerNormal = transducer.getDirection();
                                Vec2 incomingDir = returnDir * -1.0;  // Ray arriving at transducer
                                double receiveAngleCos = std::abs(incomingDir.dot(transducerNormal));

                                constexpr double MIN_ECHO_AMPLITUDE = 0.01;
                                double echoAmplitude = seg->amplitude * gain * receiveAngleCos;

                                if (echoAmplitude > MIN_ECHO_AMPLITUDE) {
                                    echoes.emplace_back(totalTOF, echoAmplitude, reflectionPoint, seg->waveType);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Sort by time of flight
        std::sort(echoes.begin(), echoes.end(),
                  [](const Echo& a, const Echo& b) { return a.timeOfFlight < b.timeOfFlight; });

        // Filter to range window (remove echoes outside display range)
        echoes.erase(
            std::remove_if(echoes.begin(), echoes.end(),
                [this](const Echo& echo) {
                    return echo.timeOfFlight < delay || echo.timeOfFlight > delay + range;
                }),
            echoes.end()
        );
    }

    // Get amplitude at a specific time (for envelope display)
    double getAmplitudeAt(double time) const {
        // Simple approach: find nearest echo
        double maxAmp = 0.0;
        double timeWindow = 0.5;  // μs window

        for (const auto& echo : echoes) {
            if (std::abs(echo.timeOfFlight - time) < timeWindow) {
                maxAmp = std::max(maxAmp, echo.amplitude);
            }
        }

        return maxAmp;
    }

    // Get the echo nearest to a given time
    const Echo* getEchoNear(double time, double tolerance = 1.0) const {
        const Echo* nearest = nullptr;
        double minDist = tolerance;

        for (const auto& echo : echoes) {
            double dist = std::abs(echo.timeOfFlight - time);
            if (dist < minDist) {
                minDist = dist;
                nearest = &echo;
            }
        }

        return nearest;
    }
};

} // namespace BeamCast
