#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include "MathTypes.h"
#include "Geometry.h"
#include <vector>
#include <string>

namespace BeamCast {

// Forward declarations
struct RaySegment;

// Color structure
struct Color {
    uint8_t r, g, b, a;

    Color(uint8_t r_ = 255, uint8_t g_ = 255, uint8_t b_ = 255, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    static Color fromHex(uint32_t hex) {
        return Color(
            (hex >> 16) & 0xFF,
            (hex >> 8) & 0xFF,
            hex & 0xFF,
            255
        );
    }
};

// Simple theme system
struct Theme {
    Color background;
    Color gridLines;
    Color geometryFill;
    Color geometryOutline;
    Color rayPath;
    Color text;

    // Dark theme (default)
    static Theme Dark() {
        Theme t;
        t.background = Color(30, 30, 35, 255);
        t.gridLines = Color(50, 50, 55, 255);
        t.geometryFill = Color(80, 100, 120, 180);
        t.geometryOutline = Color(120, 140, 160, 255);
        t.rayPath = Color(100, 200, 255, 255);
        t.text = Color(220, 220, 220, 255);
        return t;
    }

    // Light theme
    static Theme Light() {
        Theme t;
        t.background = Color(245, 245, 248, 255);
        t.gridLines = Color(220, 220, 225, 255);
        t.geometryFill = Color(180, 190, 200, 180);
        t.geometryOutline = Color(100, 110, 130, 255);
        t.rayPath = Color(50, 100, 200, 255);
        t.text = Color(30, 30, 30, 255);
        return t;
    }
};

class Renderer {
private:
    SDL_Renderer* sdlRenderer;
    Theme currentTheme;
    TTF_Font* font;

    // Viewport transform
    Vec2 viewOffset;   // Offset in world coordinates
    double viewScale;   // Pixels per mm

public:
    Renderer(SDL_Renderer* renderer)
        : sdlRenderer(renderer)
        , currentTheme(Theme::Dark())
        , font(nullptr)
        , viewOffset(0, 0)
        , viewScale(2.0)  // 2 pixels per mm initially
    {}

    ~Renderer() {
        if (font) {
            TTF_CloseFont(font);
        }
    }

    bool initFont(const char* fontPath, int fontSize) {
        font = TTF_OpenFont(fontPath, fontSize);
        return font != nullptr;
    }

    void setTheme(const Theme& theme) { currentTheme = theme; }
    const Theme& getTheme() const { return currentTheme; }

    // Zoom controls
    void zoom(double factor) {
        viewScale *= factor;
        viewScale = Math::clamp(viewScale, 0.5, 10.0);  // Clamp between 0.5 and 10 pixels per mm
    }
    double getViewScale() const { return viewScale; }

    // Pan controls
    void setViewOffset(const Vec2& offset) { viewOffset = offset; }
    Vec2 getViewOffset() const { return viewOffset; }

    // Transform world coordinates to screen coordinates
    Vec2 worldToScreen(const Vec2& worldPos, int screenWidth, int screenHeight) const {
        // Center the viewport
        Vec2 centered = worldPos - viewOffset;
        // Scale and flip Y (SDL has Y down, we want Y up)
        return Vec2(
            screenWidth / 2.0 + centered.x * viewScale,
            screenHeight / 2.0 - centered.y * viewScale
        );
    }

    // Transform screen coordinates to world coordinates
    Vec2 screenToWorld(const Vec2& screenPos, int screenWidth, int screenHeight) const {
        Vec2 centered(
            (screenPos.x - screenWidth / 2.0) / viewScale,
            -(screenPos.y - screenHeight / 2.0) / viewScale
        );
        return centered + viewOffset;
    }

    // Clear screen
    void clear() {
        SDL_SetRenderDrawColor(sdlRenderer,
            currentTheme.background.r,
            currentTheme.background.g,
            currentTheme.background.b,
            currentTheme.background.a);
        SDL_RenderClear(sdlRenderer);
    }

    // Draw grid
    void drawGrid(int screenWidth, int screenHeight, double gridSpacing = 10.0) {
        SDL_SetRenderDrawColor(sdlRenderer,
            currentTheme.gridLines.r,
            currentTheme.gridLines.g,
            currentTheme.gridLines.b,
            currentTheme.gridLines.a);

        // Calculate grid bounds in world space
        Vec2 topLeft = screenToWorld(Vec2(0, 0), screenWidth, screenHeight);
        Vec2 bottomRight = screenToWorld(Vec2(screenWidth, screenHeight), screenWidth, screenHeight);

        // Vertical lines
        double startX = std::floor(topLeft.x / gridSpacing) * gridSpacing;
        for (double x = startX; x <= bottomRight.x; x += gridSpacing) {
            Vec2 top = worldToScreen(Vec2(x, topLeft.y), screenWidth, screenHeight);
            Vec2 bottom = worldToScreen(Vec2(x, bottomRight.y), screenWidth, screenHeight);
            SDL_RenderDrawLine(sdlRenderer, (int)top.x, (int)top.y, (int)bottom.x, (int)bottom.y);
        }

        // Horizontal lines
        double startY = std::floor(bottomRight.y / gridSpacing) * gridSpacing;
        for (double y = startY; y <= topLeft.y; y += gridSpacing) {
            Vec2 left = worldToScreen(Vec2(topLeft.x, y), screenWidth, screenHeight);
            Vec2 right = worldToScreen(Vec2(bottomRight.x, y), screenWidth, screenHeight);
            SDL_RenderDrawLine(sdlRenderer, (int)left.x, (int)left.y, (int)right.x, (int)right.y);
        }
    }

    // Draw line
    void drawLine(const Vec2& start, const Vec2& end, const Color& color, int screenW, int screenH) {
        SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);
        Vec2 p1 = worldToScreen(start, screenW, screenH);
        Vec2 p2 = worldToScreen(end, screenW, screenH);
        SDL_RenderDrawLine(sdlRenderer, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y);
    }

    // Draw filled polygon
    void drawPolygon(const std::vector<Vec2>& vertices, const Color& fillColor,
                     const Color& outlineColor, int screenW, int screenH) {
        if (vertices.size() < 3) return;

        // Convert to screen coordinates
        std::vector<Vec2> screenVerts;
        for (const auto& v : vertices) {
            screenVerts.push_back(worldToScreen(v, screenW, screenH));
        }

        // Simple triangle fan fill (works for convex polygons)
        SDL_SetRenderDrawColor(sdlRenderer, fillColor.r, fillColor.g, fillColor.b, fillColor.a);

        // SDL doesn't have filled polygon, so we'll draw it as triangles
        for (size_t i = 1; i < screenVerts.size() - 1; i++) {
            drawFilledTriangle(screenVerts[0], screenVerts[i], screenVerts[i + 1], fillColor);
        }

        // Draw outline
        SDL_SetRenderDrawColor(sdlRenderer, outlineColor.r, outlineColor.g, outlineColor.b, outlineColor.a);
        for (size_t i = 0; i < screenVerts.size(); i++) {
            Vec2 p1 = screenVerts[i];
            Vec2 p2 = screenVerts[(i + 1) % screenVerts.size()];
            SDL_RenderDrawLine(sdlRenderer, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y);
        }
    }

    // Draw geometry object
    void drawGeometry(const GeometryObject& obj, int screenW, int screenH) {
        auto vertices = obj.getVertices();
        drawPolygon(vertices, currentTheme.geometryFill, currentTheme.geometryOutline, screenW, screenH);
    }

    // Draw circle (for transducer face)
    void drawCircle(const Vec2& center, double radius, const Color& color, int screenW, int screenH, bool filled = false) {
        Vec2 screenCenter = worldToScreen(center, screenW, screenH);
        int screenRadius = (int)(radius * viewScale);

        SDL_SetRenderDrawColor(sdlRenderer, color.r, color.g, color.b, color.a);

        // Midpoint circle algorithm
        int x = screenRadius;
        int y = 0;
        int radiusError = 1 - x;

        while (x >= y) {
            // Draw 8 octants
            auto plot = [&](int px, int py) {
                int sx = (int)screenCenter.x + px;
                int sy = (int)screenCenter.y + py;
                SDL_RenderDrawPoint(sdlRenderer, sx, sy);
            };

            plot(x, y);
            plot(y, x);
            plot(-x, y);
            plot(-y, x);
            plot(-x, -y);
            plot(-y, -x);
            plot(x, -y);
            plot(y, -x);

            y++;
            if (radiusError < 0) {
                radiusError += 2 * y + 1;
            } else {
                x--;
                radiusError += 2 * (y - x + 1);
            }
        }
    }

    // Draw transducer
    void drawTransducer(const class Transducer& transducer, int screenW, int screenH, double beamSpreadDegrees = 20.0, bool isSnapped = false);

    // Draw ray segments
    void drawRaySegment(const struct RaySegment& segment, int screenW, int screenH);

    // Draw A-scan display
    void drawAScan(const class AScan& ascan, int x, int y, int width, int height, const class Transducer& transducer);

    // Draw text
    void drawText(const std::string& text, int x, int y, const Color& color) {
        if (!font) return;

        SDL_Color sdlColor = {color.r, color.g, color.b, color.a};
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), sdlColor);
        if (!surface) return;

        SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
        if (texture) {
            SDL_Rect destRect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(sdlRenderer, texture, nullptr, &destRect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }

    void present() {
        SDL_RenderPresent(sdlRenderer);
    }

private:
    void drawFilledTriangle(const Vec2& p1, const Vec2& p2, const Vec2& p3, const Color& color);  // Implemented in Renderer.cpp
};

} // namespace BeamCast
