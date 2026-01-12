#pragma once

#include "MathTypes.h"
#include "Geometry.h"
#include "Transducer.h"
#include <vector>
#include <memory>
#include <limits>

namespace BeamCast {

// Result of finding nearest surface
struct SurfaceSnap {
    bool found;
    Vec2 snapPoint;        // Point on surface
    Vec2 surfaceNormal;    // Normal pointing outward from surface
    double distance;       // Distance from query point to surface
    const GeometryObject* object;
    int edgeIndex;

    SurfaceSnap()
        : found(false)
        , snapPoint(0, 0)
        , surfaceNormal(0, 1)
        , distance(std::numeric_limits<double>::infinity())
        , object(nullptr)
        , edgeIndex(-1) {}
};

class SnappingSystem {
public:
    double snapDistance;   // Maximum distance for snapping (mm)
    bool snapEnabled;

    SnappingSystem()
        : snapDistance(15.0)  // Snap within 15mm
        , snapEnabled(true)
    {}

    // Find nearest surface point to a position
    SurfaceSnap findNearestSurface(const Vec2& point,
                                   const std::vector<std::unique_ptr<GeometryObject>>& geometries) const {
        SurfaceSnap nearest;

        for (const auto& geom : geometries) {
            auto vertices = geom->getVertices();

            // Check each edge
            for (size_t i = 0; i < vertices.size(); i++) {
                Vec2 v1 = vertices[i];
                Vec2 v2 = vertices[(i + 1) % vertices.size()];

                // Find closest point on line segment to query point
                Vec2 closestPoint = closestPointOnSegment(point, v1, v2);
                double dist = point.distanceTo(closestPoint);

                if (dist < nearest.distance) {
                    // Calculate surface normal (perpendicular to edge)
                    Vec2 edgeDir = (v2 - v1).normalized();
                    Vec2 normal = edgeDir.perpendicular();

                    // Determine which side of the edge the point is on
                    Vec2 toPoint = point - closestPoint;
                    double side = toPoint.dot(normal);

                    // Ensure normal points AWAY from polygon interior (outward)
                    // For closed polygon, check if normal points toward or away from centroid
                    Vec2 centroid(0, 0);
                    for (const auto& v : vertices) {
                        centroid = centroid + v;
                    }
                    centroid = centroid / (double)vertices.size();

                    Vec2 edgeMidpoint = (v1 + v2) * 0.5;
                    Vec2 fromCentroid = edgeMidpoint - centroid;

                    // If normal points away from centroid, it's outward-facing
                    if (fromCentroid.dot(normal) < 0) {
                        normal = normal * -1.0;
                        side = -side;  // Flip side too since we flipped the normal
                    }

                    // CRITICAL: Only consider this surface if point is on the OUTSIDE
                    // (i.e., on the side the outward normal points to)
                    if (side > 0) {
                        nearest.found = true;
                        nearest.snapPoint = closestPoint;
                        nearest.distance = dist;
                        nearest.object = geom.get();
                        nearest.edgeIndex = (int)i;
                        nearest.surfaceNormal = normal;
                    }
                }
            }
        }

        return nearest;
    }

    // Snap transducer to nearest surface
    // Returns true if snapped, updates transducer position and angle
    // If preserveRotation is true, only snaps position, keeps existing angle
    bool snapTransducerToSurface(Transducer& transducer,
                                const std::vector<std::unique_ptr<GeometryObject>>& geometries,
                                bool preserveRotation = false) const {
        if (!snapEnabled) return false;

        SurfaceSnap snap = findNearestSurface(transducer.position, geometries);

        if (snap.found && snap.distance <= snapDistance) {
            // Place transducer slightly INSIDE the material for proper contact
            // Negative offset (into material) ensures transducer is unambiguously inside
            // This allows rays to start in the material and propagate correctly
            constexpr double CONTACT_OFFSET_MM = -0.1;  // 0.1mm inside material
            transducer.position = snap.snapPoint + snap.surfaceNormal * CONTACT_OFFSET_MM;

            // Only update rotation if not preserving it
            if (!preserveRotation) {
                // Rotate aperture to be perpendicular to surface
                // Normal points outward, inward direction is where beam will go
                Vec2 inwardDir = snap.surfaceNormal * -1.0;

                // Set aperture orientation to be parallel to surface
                // Aperture normal should point inward (perpendicular to surface)
                transducer.angle = std::atan2(-inwardDir.x, inwardDir.y);

                // Calculate refracted beam angle for this material using Snell's law
                // If wedge angle is non-zero, apply refraction
                if (std::abs(transducer.wedgeAngle) > 1e-6 && snap.object) {
                    double refractedAngle = transducer.calculateRefractedAngle(snap.object->material, false);
                    if (refractedAngle >= 0.0) {
                        transducer.beamAngle = refractedAngle;
                    } else {
                        // Total internal reflection - can't propagate at this angle
                        transducer.beamAngle = 0.0;
                    }
                } else {
                    // Straight beam probe (no wedge) - beam perpendicular to aperture
                    transducer.beamAngle = 0.0;
                }
            }

            return true;
        }

        return false;
    }

    // Snap geometry to nearest vertex or edge of other geometry
    // Snaps vertices of moving geometry to vertices/edges of other geometry (edge-to-edge)
    Vec2 snapPointToGeometry(const Vec2& point,
                            const GeometryObject* exclude,
                            const std::vector<std::unique_ptr<GeometryObject>>& geometries) const {
        if (!snapEnabled) return point;

        // Get vertices of the object being moved (in world space)
        auto movingVertices = exclude->getVertices();

        Vec2 bestSnapOffset(0, 0);
        double minDist = snapDistance;
        bool foundSnap = false;

        // For each vertex of the moving object
        for (const auto& movingVertex : movingVertices) {
            // Check against all other geometry
            for (const auto& geom : geometries) {
                if (geom.get() == exclude) continue;  // Don't snap to self

                auto targetVertices = geom->getVertices();

                // Snap to vertices
                for (const auto& targetVertex : targetVertices) {
                    double dist = movingVertex.distanceTo(targetVertex);
                    if (dist < minDist) {
                        bestSnapOffset = targetVertex - movingVertex;
                        minDist = dist;
                        foundSnap = true;
                    }
                }

                // Snap to edges
                for (size_t i = 0; i < targetVertices.size(); i++) {
                    Vec2 v1 = targetVertices[i];
                    Vec2 v2 = targetVertices[(i + 1) % targetVertices.size()];

                    Vec2 closestPoint = closestPointOnSegment(movingVertex, v1, v2);
                    double dist = movingVertex.distanceTo(closestPoint);

                    if (dist < minDist) {
                        bestSnapOffset = closestPoint - movingVertex;
                        minDist = dist;
                        foundSnap = true;
                    }
                }
            }
        }

        if (foundSnap) {
            return point + bestSnapOffset;
        }

        return point;
    }

private:
    // Find closest point on a line segment to a query point
    Vec2 closestPointOnSegment(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) const {
        Vec2 segDir = segEnd - segStart;
        double segLength = segDir.length();

        if (segLength < 1e-10) {
            return segStart;  // Degenerate segment
        }

        Vec2 segNorm = segDir / segLength;
        Vec2 pointToStart = point - segStart;

        // Project point onto line
        double t = pointToStart.dot(segNorm);

        // Clamp to segment
        t = Math::clamp(t, 0.0, segLength);

        return segStart + segNorm * t;
    }
};

} // namespace BeamCast
