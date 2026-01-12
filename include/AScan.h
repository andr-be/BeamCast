#pragma once

#include "MathTypes.h"
#include "RayTracer.h"
#include "PhysicsConstants.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>

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
    double gainDB;       // Gain in decibels (0-110 dB, typical UT range)

    // Display modes
    enum RectificationMode {
        ENVELOPE,        // Full rectified envelope (current display)
        HALF_WAVE,       // Half-wave rectified (positive only)
        RF_MODE          // Full RF waveform (±100% bipolar)
    };
    RectificationMode rectificationMode;

    AScan()
        : range(200.0)   // 200μs default range - covers longer path lengths
        , delay(0.0)
        , gainDB(0.0)    // 0 dB default (×1.0 linear, no amplification) - user adjusts up
        , rectificationMode(ENVELOPE)
    {}

    // Convert gain from dB to linear multiplier
    double getGainLinear() const {
        return std::pow(10.0, gainDB / 20.0);
    }

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
                        // This is a reflection point - check if return path intersects transducer aperture
                        // Transducer aperture is a line segment perpendicular to beam direction

                        Vec2 reflectionPoint = seg->end;
                        Vec2 returnDir = dir2;

                        // Get transducer aperture line segment
                        Vec2 apertureP1, apertureP2;
                        transducer.getApertureSegment(apertureP1, apertureP2);

                        // Ray-line segment intersection
                        // Ray: P = reflectionPoint + t * returnDir  (t >= 0)
                        // Segment: Q = apertureP1 + s * (apertureP2 - apertureP1)  (0 <= s <= 1)
                        Vec2 apertureDir = apertureP2 - apertureP1;
                        Vec2 originToAperture = apertureP1 - reflectionPoint;

                        double cross = returnDir.x * apertureDir.y - returnDir.y * apertureDir.x;

                        bool intersects = false;
                        double t = 0.0;
                        Vec2 intersectionPoint;

                        if (std::abs(cross) > 1e-10) {  // Not parallel
                            t = (originToAperture.x * apertureDir.y - originToAperture.y * apertureDir.x) / cross;
                            double s = (originToAperture.x * returnDir.y - originToAperture.y * returnDir.x) / cross;

                            if (t >= 0.0 && s >= 0.0 && s <= 1.0) {  // Valid intersection
                                intersects = true;
                                intersectionPoint = reflectionPoint + returnDir * t;
                            }
                        }

                        if (intersects && t > 0) {  // Intersection ahead of reflection point
                                // Valid echo - calculate return path TOF
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
                                double receiveAngleCos = incomingDir.dot(transducerNormal);

                                // Only accept echoes hitting transducer face (positive dot product)
                                // Negative means ray is hitting from behind
                                if (receiveAngleCos <= 0.0) continue;

                                // Amplitude falls off with angle (cosine response)
                                constexpr double MIN_ECHO_AMPLITUDE = 0.01;
                                double gainLinear = getGainLinear();

                                // Apply preamplifier sensitivity (transducer electromechanical efficiency)
                                // This represents the combined transmit + receive conversion efficiency
                                double echoAmplitude = seg->amplitude * TransducerConstants::PREAMPLIFIER_SENSITIVITY
                                                     * gainLinear * receiveAngleCos;

                                // Saturation at 100% FSH (Full Screen Height)
                                echoAmplitude = std::min(1.0, echoAmplitude);

                                if (echoAmplitude > MIN_ECHO_AMPLITUDE) {
                                    echoes.emplace_back(totalTOF, echoAmplitude, reflectionPoint, seg->waveType);
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

    // Synthesize RF waveform at a specific time
    // Returns amplitude in range [-1, +1] for RF mode
    double synthesizeRFWaveform(double time_us, double frequency_MHz, double bandwidth, uint32_t frameSeed) const {
        double totalAmplitude = 0.0;

        // Pulse duration from bandwidth: Δt ≈ 1/Δf where Δf = f × bandwidth
        double pulseDuration_us = 1.0 / (frequency_MHz * bandwidth);

        // Initial pulse ringdown (transducer excitation at t=0)
        // This represents the transducer resonance decay - always present
        // Causes the "dead zone" where near-surface echoes are masked
        double initialPulseTime = time_us - delay;  // Time relative to t=0
        if (initialPulseTime >= 0 && initialPulseTime < pulseDuration_us * 3.0) {
            double alpha = bandwidth * frequency_MHz * 2.0;
            double damping = std::exp(-alpha * initialPulseTime);
            double phase = 2.0 * M_PI * frequency_MHz * initialPulseTime;
            double oscillation = std::sin(phase);
            // Initial pulse at full amplitude (100% FSH before gain)
            totalAmplitude += damping * oscillation;
        }

        for (const auto& echo : echoes) {
            double timeDelta = time_us - echo.timeOfFlight;

            // Only process echoes that could contribute at this time
            if (std::abs(timeDelta) > pulseDuration_us * 3.0) continue;  // 3x pulse duration window

            // Damped sinusoidal pulse: A × exp(-α·|t-t0|) × cos(2πf·(t-t0))
            // Using cosine so pulse peaks at TOF (not zero-crossing)
            // Damping factor α based on bandwidth (higher bandwidth = more damping)
            double alpha = bandwidth * frequency_MHz * 2.0;  // Empirical damping rate
            double damping = std::exp(-alpha * std::abs(timeDelta));

            // Oscillation at center frequency - cosine so amplitude peaks at t=TOF
            double phase = 2.0 * M_PI * frequency_MHz * timeDelta;
            double oscillation = std::cos(phase);

            // Combine: amplitude × damping × oscillation
            totalAmplitude += echo.amplitude * damping * oscillation;
        }

        // Add electronic noise (gain-dependent)
        // Generate pseudo-random noise that changes each frame
        // Use better hash mixing to avoid saw-wave patterns
        uint32_t timeSeed = (uint32_t)(time_us * 10000.0);  // Higher precision
        uint32_t noiseHash = frameSeed * 2654435761u + timeSeed;  // Mix frame and time
        noiseHash ^= noiseHash >> 16;
        noiseHash *= 0x85ebca6bu;
        noiseHash ^= noiseHash >> 13;
        noiseHash *= 0xc2b2ae35u;
        noiseHash ^= noiseHash >> 16;
        double randomValue = (noiseHash & 0xFFFFFF) / 16777216.0;  // [0, 1)
        double noise = (randomValue - 0.5) * 2.0;  // Convert to [-1, +1]

        // Scale noise by gain level
        double gainLinear = getGainLinear();
        double noiseAmplitude = AScanDisplay::BASE_NOISE_AMPLITUDE * std::pow(gainLinear, AScanDisplay::NOISE_GAIN_SCALING);
        totalAmplitude += noise * noiseAmplitude;

        // Clamp to ±1.0
        return std::max(-1.0, std::min(1.0, totalAmplitude));
    }

    // Get envelope (rectified) amplitude at a specific time
    double getEnvelopeAt(double time_us, double frequency_MHz, double bandwidth, uint32_t frameSeed) const {
        double totalAmplitude = 0.0;

        double pulseDuration_us = 1.0 / (frequency_MHz * bandwidth);

        // Initial pulse ringdown (transducer excitation at t=0)
        // Envelope mode shows just the damping curve
        double initialPulseTime = time_us - delay;  // Time relative to t=0
        if (initialPulseTime >= 0 && initialPulseTime < pulseDuration_us * 3.0) {
            double alpha = bandwidth * frequency_MHz * 2.0;
            double damping = std::exp(-alpha * initialPulseTime);
            // Initial pulse at full amplitude (100% FSH before gain)
            totalAmplitude += damping;
        }

        for (const auto& echo : echoes) {
            double timeDelta = time_us - echo.timeOfFlight;

            if (std::abs(timeDelta) > pulseDuration_us * 3.0) continue;

            // Envelope is just the damping curve without oscillation
            double alpha = bandwidth * frequency_MHz * 2.0;
            double damping = std::exp(-alpha * std::abs(timeDelta));

            totalAmplitude += echo.amplitude * damping;
        }

        // Add electronic noise (gain-dependent)
        // Generate pseudo-random noise that changes each frame
        // Use better hash mixing to avoid saw-wave patterns
        uint32_t timeSeed = (uint32_t)(time_us * 10000.0);  // Higher precision
        uint32_t noiseHash = frameSeed * 2654435761u + timeSeed;  // Mix frame and time
        noiseHash ^= noiseHash >> 16;
        noiseHash *= 0x85ebca6bu;
        noiseHash ^= noiseHash >> 13;
        noiseHash *= 0xc2b2ae35u;
        noiseHash ^= noiseHash >> 16;
        double randomValue = (noiseHash & 0xFFFFFF) / 16777216.0;  // [0, 1)
        double noise = (randomValue - 0.5) * 2.0;  // Convert to [-1, +1]

        // Scale noise by gain level
        double gainLinear = getGainLinear();
        double noiseAmplitude = AScanDisplay::BASE_NOISE_AMPLITUDE * std::pow(gainLinear, AScanDisplay::NOISE_GAIN_SCALING);

        // For envelope, add absolute value of noise (rectified)
        totalAmplitude += std::abs(noise) * noiseAmplitude;

        // Clamp to [0, 1.0]
        return std::min(1.0, totalAmplitude);
    }
};

} // namespace BeamCast
