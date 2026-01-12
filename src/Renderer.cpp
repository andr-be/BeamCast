#include "Renderer.h"
#include "Transducer.h"
#include "RayTracer.h"
#include "AScan.h"
#include <algorithm>

namespace BeamCast {

// Helper: Fill a triangle using scanline algorithm
static void fillTriangleScanline(SDL_Renderer* renderer, const Vec2& v0, const Vec2& v1, const Vec2& v2) {
    // Sort vertices by Y coordinate (v0.y <= v1.y <= v2.y)
    Vec2 vertices[3] = {v0, v1, v2};

    if (vertices[1].y < vertices[0].y) std::swap(vertices[0], vertices[1]);
    if (vertices[2].y < vertices[0].y) std::swap(vertices[0], vertices[2]);
    if (vertices[2].y < vertices[1].y) std::swap(vertices[1], vertices[2]);

    const Vec2& p0 = vertices[0];
    const Vec2& p1 = vertices[1];
    const Vec2& p2 = vertices[2];

    // Degenerate triangle check
    if (std::abs(p2.y - p0.y) < 0.5) return;

    // Scanline fill
    for (int y = (int)std::ceil(p0.y); y <= (int)p2.y; y++) {
        double ty = (y - p0.y) / (p2.y - p0.y);  // Parameter along p0->p2
        double x_long = p0.x + ty * (p2.x - p0.x);  // X coordinate on long edge (p0->p2)

        double x_short;
        if (y <= p1.y) {
            // Top half: interpolate along p0->p1
            if (std::abs(p1.y - p0.y) < 0.5) {
                x_short = p0.x;
            } else {
                double t = (y - p0.y) / (p1.y - p0.y);
                x_short = p0.x + t * (p1.x - p0.x);
            }
        } else {
            // Bottom half: interpolate along p1->p2
            if (std::abs(p2.y - p1.y) < 0.5) {
                x_short = p1.x;
            } else {
                double t = (y - p1.y) / (p2.y - p1.y);
                x_short = p1.x + t * (p2.x - p1.x);
            }
        }

        // Draw horizontal line from min(x) to max(x)
        int x_start = (int)std::min(x_long, x_short);
        int x_end = (int)std::max(x_long, x_short);
        SDL_RenderDrawLine(renderer, x_start, y, x_end, y);
    }
}

void Renderer::drawTransducer(const Transducer& transducer, int screenW, int screenH, double beamSpreadDegrees, bool isSnapped) {
    Vec2 pos = transducer.position;
    Vec2 dir = transducer.getDirection();

    // Transducer visualization:
    // - Circle for element face
    // - Line showing direction
    // - Beam cone showing beam spread

    double faceRadius = transducer.elementDiameter / 2.0;

    // Draw beam cone outline first (behind everything else)
    // Use distinct yellow/orange color to differentiate from rays
    double coneLength = RenderingConstants::BEAM_CONE_VISUAL_LENGTH_MM;
    double halfSpreadRadians = Math::toRadians(beamSpreadDegrees / 2.0);

    Vec2 coneLeft = dir.rotated(-halfSpreadRadians) * coneLength;
    Vec2 coneRight = dir.rotated(halfSpreadRadians) * coneLength;

    Color coneColor = Color(255, 200, 50, 120);  // Yellow-orange, semi-transparent
    drawLine(pos, pos + coneLeft, coneColor, screenW, screenH);
    drawLine(pos, pos + coneRight, coneColor, screenW, screenH);

    // Draw element face
    Color faceColor = Color(255, 180, 100, 255);  // Orange-ish
    drawCircle(pos, faceRadius, faceColor, screenW, screenH);

    // Draw direction indicator (line from center)
    double indicatorLength = RenderingConstants::TRANSDUCER_DIRECTION_INDICATOR_LENGTH_MM;
    Vec2 dirEnd = pos + dir * indicatorLength;
    drawLine(pos, dirEnd, Color(255, 220, 120, 255), screenW, screenH);

    // Draw center dot
    Color centerColor = Color(255, 100, 50, 255);
    Vec2 screenPos = worldToScreen(pos, screenW, screenH);
    SDL_SetRenderDrawColor(sdlRenderer, centerColor.r, centerColor.g, centerColor.b, centerColor.a);
    SDL_Rect centerRect = {
        (int)screenPos.x - 2,
        (int)screenPos.y - 2,
        4, 4
    };
    SDL_RenderFillRect(sdlRenderer, &centerRect);
}

void Renderer::drawRaySegment(const RaySegment& segment, int screenW, int screenH) {
    // Opacity scales more dramatically with amplitude so threshold is meaningful
    // Use quadratic scaling: weak rays (near threshold) are very faint
    // Strong rays (near 100%) are bright and visible
    double amplitudeSquared = segment.amplitude * segment.amplitude;
    uint8_t alpha = RenderingConstants::MIN_RAY_ALPHA +
        (uint8_t)(amplitudeSquared * (RenderingConstants::MAX_RAY_ALPHA - RenderingConstants::MIN_RAY_ALPHA));

    Color rayColor = (segment.waveType == Ray::LONGITUDINAL) ?
        Color(100, 200, 255, alpha) :  // Blue for L-waves
        Color(255, 100, 200, alpha);   // Pink for S-waves

    drawLine(segment.start, segment.end, rayColor, screenW, screenH);
}

void Renderer::drawAScan(const AScan& ascan, int x, int y, int width, int height, const class Transducer& transducer) {
    // Draw A-scan panel background
    Color panelBg = Color(40, 40, 45, 255);
    SDL_SetRenderDrawColor(sdlRenderer, panelBg.r, panelBg.g, panelBg.b, panelBg.a);
    SDL_Rect panelRect = {x, y, width, height};
    SDL_RenderFillRect(sdlRenderer, &panelRect);

    // Draw border
    Color borderColor = currentTheme.geometryOutline;
    SDL_SetRenderDrawColor(sdlRenderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(sdlRenderer, &panelRect);

    // Draw grid lines (time divisions)
    Color gridColor = Color(60, 60, 65, 255);
    SDL_SetRenderDrawColor(sdlRenderer, gridColor.r, gridColor.g, gridColor.b, gridColor.a);

    int numDivisions = 10;
    for (int i = 0; i <= numDivisions; i++) {
        int lineX = x + (width * i) / numDivisions;
        SDL_RenderDrawLine(sdlRenderer, lineX, y, lineX, y + height);
    }

    // Horizontal grid (amplitude divisions)
    for (int i = 0; i <= 4; i++) {
        int lineY = y + (height * i) / 4;
        SDL_RenderDrawLine(sdlRenderer, x, lineY, x + width, lineY);
    }

    // Draw baseline (center for RF mode, bottom for envelope modes)
    int baselineY = (ascan.rectificationMode == AScan::RF_MODE) ? (y + height / 2) : (y + height - 1);
    Color baselineColor = Color(100, 100, 100, 255);
    SDL_SetRenderDrawColor(sdlRenderer, baselineColor.r, baselineColor.g, baselineColor.b, baselineColor.a);
    SDL_RenderDrawLine(sdlRenderer, x, baselineY, x + width, baselineY);

    // Draw waveform based on rectification mode
    if (ascan.rectificationMode == AScan::RF_MODE) {
        // RF Mode: Full oscillating waveform (±100%)
        Color waveformColor = Color(100, 255, 100, 255);  // Green
        SDL_SetRenderDrawColor(sdlRenderer, waveformColor.r, waveformColor.g, waveformColor.b, waveformColor.a);

        int prevScreenX = -1;
        int prevScreenY = baselineY;

        for (int screenX = 0; screenX < width; screenX++) {
            // Convert screen X to time
            double normalizedX = (double)screenX / width;
            double time_us = ascan.delay + normalizedX * ascan.range;

            // Synthesize RF waveform
            double amplitude = ascan.synthesizeRFWaveform(time_us, transducer.frequency, transducer.bandwidth);

            // Convert amplitude to screen Y (±50% of height from baseline)
            int amplitudeHeight = (int)(amplitude * height * 0.5 * AScanDisplay::MAX_AMPLITUDE_DISPLAY_FRACTION);
            int screenY = baselineY - amplitudeHeight;  // Negative amp goes down, positive goes up

            // Draw line segment
            if (prevScreenX >= 0) {
                SDL_RenderDrawLine(sdlRenderer, x + prevScreenX, prevScreenY, x + screenX, screenY);
            }

            prevScreenX = screenX;
            prevScreenY = screenY;
        }
    } else {
        // ENVELOPE or HALF_WAVE: Rectified modes
        Color waveformColor = Color(100, 255, 100, 255);  // Green
        SDL_SetRenderDrawColor(sdlRenderer, waveformColor.r, waveformColor.g, waveformColor.b, waveformColor.a);

        int prevScreenX = -1;
        int prevScreenY = baselineY;

        for (int screenX = 0; screenX < width; screenX++) {
            // Convert screen X to time
            double normalizedX = (double)screenX / width;
            double time_us = ascan.delay + normalizedX * ascan.range;

            // Get envelope amplitude
            double amplitude = ascan.getEnvelopeAt(time_us, transducer.frequency, transducer.bandwidth);

            // Check for saturation
            bool isSaturated = (amplitude >= 0.99);

            // Convert amplitude to height
            int amplitudeHeight = (int)(amplitude * height * AScanDisplay::MAX_AMPLITUDE_DISPLAY_FRACTION);
            int screenY = baselineY - amplitudeHeight;  // Draw upward from baseline

            // Color based on saturation
            Color color = isSaturated ? Color(255, 100, 100, 255) : Color(100, 255, 100, 255);
            SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);

            // Draw line segment
            if (prevScreenX >= 0) {
                SDL_RenderDrawLine(sdlRenderer, x + prevScreenX, prevScreenY, x + screenX, screenY);
            }

            prevScreenX = screenX;
            prevScreenY = screenY;
        }
    }

    // Draw axis labels (simple text placeholders - proper text rendering would need SDL_ttf)
    // For now, just draw markers
}

void Renderer::drawFilledTriangle(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Color& color) {
    SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
    fillTriangleScanline(sdlRenderer, p1, p2, p3);
}

} // namespace BeamCast
