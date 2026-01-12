#pragma once

#include "MathTypes.h"
#include "Material.h"
#include <vector>
#include <memory>

namespace BeamCast {

// Forward declarations
class GeometryObject;

// Base class for all geometric objects
class GeometryObject {
public:
    Material material;
    Vec2 position;  // Reference position

    GeometryObject(const Material& mat = Material())
        : material(mat), position(0, 0) {}

    virtual ~GeometryObject() = default;

    // Get vertices for rendering and ray intersection
    virtual std::vector<Vec2> getVertices() const = 0;

    // Check if a point is inside the object
    virtual bool contains(const Vec2& point) const = 0;

    // Get bounding box (for optimization later)
    virtual void getBounds(Vec2& min, Vec2& max) const = 0;
};

// Rectangle - most common geometry for basic UT testing
class Rectangle : public GeometryObject {
public:
    double width;
    double height;
    double rotation; // Rotation in radians

    Rectangle(const Vec2& pos, double w, double h, const Material& mat = Materials::Steel())
        : GeometryObject(mat)
        , width(w)
        , height(h)
        , rotation(0.0)
    {
        position = pos;
    }

    std::vector<Vec2> getVertices() const override {
        // Get corners relative to center, then rotate and translate
        std::vector<Vec2> vertices;
        vertices.reserve(4);

        // Half dimensions
        double hw = width / 2.0;
        double hh = height / 2.0;

        // Corner positions (before rotation)
        Vec2 corners[4] = {
            Vec2(-hw, -hh),  // Bottom-left
            Vec2( hw, -hh),  // Bottom-right
            Vec2( hw,  hh),  // Top-right
            Vec2(-hw,  hh)   // Top-left
        };

        // Apply rotation and translation
        for (int i = 0; i < 4; i++) {
            Vec2 rotated = corners[i].rotated(rotation);
            vertices.push_back(position + rotated);
        }

        return vertices;
    }

    bool contains(const Vec2& point) const override {
        // Transform point to local coordinates
        Vec2 local = point - position;
        local = local.rotated(-rotation);

        // Check if within bounds
        double hw = width / 2.0;
        double hh = height / 2.0;
        return (std::abs(local.x) <= hw) && (std::abs(local.y) <= hh);
    }

    void getBounds(Vec2& min, Vec2& max) const override {
        auto vertices = getVertices();
        min = vertices[0];
        max = vertices[0];

        for (const auto& v : vertices) {
            if (v.x < min.x) min.x = v.x;
            if (v.y < min.y) min.y = v.y;
            if (v.x > max.x) max.x = v.x;
            if (v.y > max.y) max.y = v.y;
        }
    }
};

// Line segment - for interfaces and edges
struct LineSegment {
    Vec2 start;
    Vec2 end;

    LineSegment(const Vec2& s, const Vec2& e) : start(s), end(e) {}

    Vec2 direction() const { return (end - start).normalized(); }
    double length() const { return start.distanceTo(end); }

    // Get normal vector (perpendicular, pointing "outward")
    Vec2 normal() const { return direction().perpendicular(); }
};

// Ray for ultrasonic pulse tracing
struct Ray {
    Vec2 origin;
    Vec2 direction;  // Should be normalized
    double amplitude; // Current amplitude (0.0 to 1.0)
    double distance;  // Distance traveled (mm)

    enum WaveType {
        LONGITUDINAL,
        SHEAR
    } waveType;

    Ray(const Vec2& origin_, const Vec2& direction_, WaveType type = LONGITUDINAL)
        : origin(origin_)
        , direction(direction_.normalized())
        , amplitude(1.0)
        , distance(0.0)
        , waveType(type)
    {}

    // Get point along ray
    Vec2 pointAt(double t) const {
        return origin + direction * t;
    }
};

} // namespace BeamCast
