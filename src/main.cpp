#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <memory>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef Rectangle  // Undefine Windows GDI Rectangle macro that conflicts with our class
#endif

#include <SDL.h>
#include <SDL_ttf.h>

#include "MathTypes.h"
#include "Material.h"
#include "Geometry.h"
#include "Renderer.h"
#include "Transducer.h"
#include "RayTracer.h"
#include "AScan.h"
#include "Snapping.h"

using namespace BeamCast;

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

int main(int argc, char* argv[]) {
    // Set console to UTF-8 for proper Unicode display (Windows)
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return EXIT_FAILURE;
    }

    // Initialize SDL_ttf
    if (TTF_Init() < 0) {
        std::cerr << "SDL_ttf initialization failed: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "BeamCast UT Simulator - MVP",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // Create SDL renderer
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!sdlRenderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::cout << "BeamCast UT Simulator initialized successfully" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  ESC   = Quit" << std::endl;
    std::cout << "  T     = Toggle theme" << std::endl;
    std::cout << "  SPACE = Run simulation" << std::endl;
    std::cout << "  A     = Toggle auto-simulate (currently ON)" << std::endl;
    std::cout << "  S     = Toggle snapping (currently ON)" << std::endl;
    std::cout << "  DRAG  = Click and drag transducer or geometry" << std::endl;
    std::cout << "  R         = Cycle probe angle (0, 45, 60, 70 deg)" << std::endl;
    std::cout << "  SHIFT+R   = Rotate last dragged object (15 deg)" << std::endl;
    std::cout << "  MOUSE WHEEL = Zoom in/out" << std::endl;
    std::cout << "  MIDDLE MOUSE = Pan view (drag)" << std::endl;
    std::cout << "\nSimulation Controls:" << std::endl;
    std::cout << "  [/]   = Decrease/Increase ray count" << std::endl;
    std::cout << "  -/=   = Decrease/Increase beam spread angle" << std::endl;
    std::cout << "  ,/.   = Decrease/Increase amplitude threshold" << std::endl;
    std::cout << "  1/2   = Decrease/Increase A-scan range" << std::endl;
    std::cout << "  3/4   = Decrease/Increase A-scan gain (dB)" << std::endl;

    // Create our renderer wrapper
    Renderer renderer(sdlRenderer);

    // Initialize font - try multiple common locations
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\consola.ttf",      // Windows: Consolas
        "C:\\Windows\\Fonts\\cour.ttf",         // Windows: Courier New
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",  // Linux: DejaVu Sans Mono
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",               // Arch Linux
        "/System/Library/Fonts/Courier.dfont",  // macOS: Courier
        nullptr
    };

    bool fontLoaded = false;
    for (int i = 0; fontPaths[i] != nullptr; i++) {
        if (renderer.initFont(fontPaths[i], 14)) {
            fontLoaded = true;
            std::cout << "Loaded font: " << fontPaths[i] << std::endl;
            break;
        }
    }

    if (!fontLoaded) {
        std::cerr << "Warning: Failed to load font from any known location. Text rendering disabled." << std::endl;
    }

    // Create ray tracer
    RayTracer rayTracer;

    // Create A-scan display
    AScan ascan;

    // Create snapping system
    SnappingSystem snapping;

    // Create calibration block set scenario
    // Common UT calibration: step blocks with increasing thickness (40-100mm range)
    // Each block is 30mm wide (diameter), arranged horizontally
    std::vector<std::unique_ptr<GeometryObject>> geometries;

    // Standard 6-bar calibration set: 40, 50, 60, 70, 80, 100mm thicknesses
    const double blockWidth = 30.0;   // 30mm diameter blocks
    const double spacing = 10.0;       // 10mm gap between blocks
    const std::vector<double> thicknesses = {40.0, 50.0, 60.0, 70.0, 80.0, 100.0};

    double xOffset = -(thicknesses.size() * (blockWidth + spacing)) / 2.0;  // Center the set

    for (size_t i = 0; i < thicknesses.size(); i++) {
        double thickness = thicknesses[i];
        double xPos = xOffset + i * (blockWidth + spacing) + blockWidth / 2.0;

        // Position blocks so their tops are aligned at y=0
        double yPos = -thickness / 2.0;

        geometries.push_back(std::make_unique<BeamCast::Rectangle>(
            Vec2(xPos, yPos),
            blockWidth,
            thickness,
            Materials::Steel()
        ));
    }

    // Create transducer - positioned above the calibration blocks
    // For thickness gauging: transducer pointing down (180°), will snap to block tops
    Transducer transducer(Vec2(-80, 15), Math::toRadians(180.0), 5.0);

    // Simulation state
    bool simulationRun = false;
    bool autoSimulate = true;  // Auto-run simulation when objects move

    // Simulation parameters (adjustable at runtime)
    int numRays = 128;
    double beamSpreadAngle = 20.0;  // degrees

    // Standard UT probe angles (degrees from surface normal)
    const std::vector<double> standardProbeAngles = {0.0, 45.0, 60.0, 70.0};
    int currentProbeAngleIndex = 0;  // Start at 0° (straight beam)

    // Interaction state
    bool isDragging = false;
    bool draggingTransducer = false;
    int draggingGeometryIndex = -1;
    int lastDraggedGeometryIndex = -1;  // Track last dragged object for rotation
    Vec2 dragOffset(0, 0);
    bool transducerSnapped = false;  // Track if transducer is currently snapped to surface

    // Panning state (middle mouse button)
    bool isPanning = false;
    Vec2 panStart(0, 0);
    Vec2 viewOffsetStart(0, 0);

    // Main loop flag
    bool running = true;
    bool useDarkTheme = true;
    SDL_Event event;

    int currentWidth = WINDOW_WIDTH;
    int currentHeight = WINDOW_HEIGHT;

    // Main loop
    while (running) {
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                else if (event.key.keysym.sym == SDLK_t) {
                    // Toggle theme
                    useDarkTheme = !useDarkTheme;
                    renderer.setTheme(useDarkTheme ? Theme::Dark() : Theme::Light());
                    std::cout << "Theme: " << (useDarkTheme ? "Dark" : "Light") << std::endl;
                }
                else if (event.key.keysym.sym == SDLK_SPACE) {
                    // Run simulation
                    std::cout << "Running simulation..." << std::endl;
                    rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                    ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    simulationRun = true;
                    std::cout << "Traced " << rayTracer.tracedPaths.size() << " ray segments" << std::endl;
                    std::cout << "Detected " << ascan.echoes.size() << " echoes" << std::endl;
                }
                else if (event.key.keysym.sym == SDLK_a) {
                    // Toggle auto-simulate
                    autoSimulate = !autoSimulate;
                    std::cout << "Auto-simulate: " << (autoSimulate ? "ON" : "OFF") << std::endl;
                }
                else if (event.key.keysym.sym == SDLK_s) {
                    // Toggle snapping
                    snapping.snapEnabled = !snapping.snapEnabled;
                    std::cout << "Snapping: " << (snapping.snapEnabled ? "ON" : "OFF") << std::endl;
                }
                else if (event.key.keysym.sym == SDLK_r) {
                    SDL_Keymod modifiers = SDL_GetModState();
                    bool shiftPressed = (modifiers & KMOD_SHIFT) != 0;

                    if (shiftPressed && lastDraggedGeometryIndex >= 0 && lastDraggedGeometryIndex < (int)geometries.size()) {
                        // Shift+R: Rotate last dragged geometry by 15° increments
                        auto* rect = dynamic_cast<BeamCast::Rectangle*>(geometries[lastDraggedGeometryIndex].get());
                        if (rect) {
                            rect->rotation += Math::toRadians(UIControls::GEOMETRY_ROTATION_INCREMENT_DEG);
                            // Normalize to [0, 2π)
                            while (rect->rotation >= 2.0 * M_PI) rect->rotation -= 2.0 * M_PI;

                            std::cout << "Geometry rotation: " << Math::toDegrees(rect->rotation) << " deg" << std::endl;

                            if (autoSimulate) {
                                rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                                ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                                simulationRun = true;
                            }
                        }
                    } else {
                        // R: Cycle through standard probe angles (0°, 45°, 60°, 70°)
                        currentProbeAngleIndex = (currentProbeAngleIndex + 1) % standardProbeAngles.size();
                        double probeAngleDegrees = standardProbeAngles[currentProbeAngleIndex];

                        // If transducer is snapped to a surface, apply rotation relative to surface
                        // Otherwise, just set absolute angle (0° = pointing up, 180° = pointing down)
                        if (transducerSnapped) {
                            // Get current surface snap to determine surface orientation
                            SurfaceSnap snap = snapping.findNearestSurface(transducer.position, geometries);
                            if (snap.found) {
                                // Calculate inward direction (perpendicular to surface, into material)
                                Vec2 inwardDir = snap.surfaceNormal * -1.0;

                                // Base angle points inward (perpendicular to surface)
                                // Using same formula as snapping: Vec2(0,1).rotated(angle) = inwardDir
                                double baseAngle = std::atan2(-inwardDir.x, inwardDir.y);

                                // Apply probe angle offset from surface normal
                                // Positive angle = clockwise from inward normal (standard UT convention)
                                transducer.angle = baseAngle + Math::toRadians(probeAngleDegrees);
                            }
                        } else {
                            // Not snapped: set absolute angle (0° = up, 180° = down)
                            transducer.angle = Math::toRadians(probeAngleDegrees);
                        }

                        if (autoSimulate) {
                            rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                            ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                            simulationRun = true;
                        }
                        std::cout << "Probe angle: " << probeAngleDegrees << " deg (" <<
                            (probeAngleDegrees == 0.0 ? "straight beam" : "angle beam") << ")" << std::endl;
                    }
                }
                // Ray count controls
                else if (event.key.keysym.sym == SDLK_LEFTBRACKET) {
                    numRays = std::max(UIControls::RAY_COUNT_MIN, numRays - UIControls::RAY_COUNT_INCREMENT);
                    std::cout << "Ray count: " << numRays << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                else if (event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                    numRays = std::min(UIControls::RAY_COUNT_MAX, numRays + UIControls::RAY_COUNT_INCREMENT);
                    std::cout << "Ray count: " << numRays << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                // Beam spread controls
                else if (event.key.keysym.sym == SDLK_MINUS) {
                    beamSpreadAngle = std::max(UIControls::BEAM_SPREAD_MIN_DEG,
                                              beamSpreadAngle - UIControls::BEAM_SPREAD_INCREMENT_DEG);
                    std::cout << "Beam spread: ±" << beamSpreadAngle << " deg" << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                else if (event.key.keysym.sym == SDLK_EQUALS) {
                    beamSpreadAngle = std::min(UIControls::BEAM_SPREAD_MAX_DEG,
                                              beamSpreadAngle + UIControls::BEAM_SPREAD_INCREMENT_DEG);
                    std::cout << "Beam spread: ±" << beamSpreadAngle << " deg" << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                // Amplitude threshold controls
                else if (event.key.keysym.sym == SDLK_COMMA) {
                    rayTracer.amplitudeThreshold = std::max(UIControls::AMPLITUDE_THRESHOLD_MIN,
                                                           rayTracer.amplitudeThreshold - UIControls::AMPLITUDE_THRESHOLD_INCREMENT);
                    std::cout << "Amplitude threshold: " << (rayTracer.amplitudeThreshold * 100) << "%" << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                else if (event.key.keysym.sym == SDLK_PERIOD) {
                    rayTracer.amplitudeThreshold = std::min(UIControls::AMPLITUDE_THRESHOLD_MAX,
                                                           rayTracer.amplitudeThreshold + UIControls::AMPLITUDE_THRESHOLD_INCREMENT);
                    std::cout << "Amplitude threshold: " << (rayTracer.amplitudeThreshold * 100) << "%" << std::endl;
                    if (autoSimulate && simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                // A-scan range controls
                else if (event.key.keysym.sym == SDLK_1) {
                    ascan.range = std::max(UIControls::ASCAN_RANGE_MIN_US,
                                          ascan.range - UIControls::ASCAN_RANGE_INCREMENT_US);
                    rayTracer.maxTimeOfFlight = ascan.range;  // Sync ray tracing time window
                    std::cout << "A-scan range: " << ascan.range << " us" << std::endl;
                    // Re-run simulation since time window changed
                    if (autoSimulate || simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                        simulationRun = true;
                    }
                }
                else if (event.key.keysym.sym == SDLK_2) {
                    ascan.range = std::min(UIControls::ASCAN_RANGE_MAX_US,
                                          ascan.range + UIControls::ASCAN_RANGE_INCREMENT_US);
                    rayTracer.maxTimeOfFlight = ascan.range;  // Sync ray tracing time window
                    std::cout << "A-scan range: " << ascan.range << " us" << std::endl;
                    // Re-run simulation since time window changed
                    if (autoSimulate || simulationRun) {
                        rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                        simulationRun = true;
                    }
                }
                // A-scan gain controls
                else if (event.key.keysym.sym == SDLK_3) {
                    ascan.gainDB = std::max(0.0, ascan.gainDB - 2.0);  // 2 dB decrements
                    std::cout << "A-scan gain: " << ascan.gainDB << " dB (x"
                              << std::fixed << std::setprecision(1) << ascan.getGainLinear() << ")" << std::endl;
                    // Re-generate A-scan with new gain
                    if (simulationRun) {
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
                else if (event.key.keysym.sym == SDLK_4) {
                    ascan.gainDB = std::min(80.0, ascan.gainDB + 2.0);  // 2 dB increments, max 80 dB
                    std::cout << "A-scan gain: " << ascan.gainDB << " dB (x"
                              << std::fixed << std::setprecision(1) << ascan.getGainLinear() << ")" << std::endl;
                    // Re-generate A-scan with new gain
                    if (simulationRun) {
                        ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Get mouse position in world coordinates
                    Vec2 mouseScreen(event.button.x, event.button.y);
                    Vec2 mouseWorld = renderer.screenToWorld(mouseScreen, currentWidth, currentHeight);

                    // Check if clicking on transducer
                    double transducerRadius = transducer.elementDiameter / 2.0 + RenderingConstants::TRANSDUCER_CLICK_MARGIN_MM;
                    if (mouseWorld.distanceTo(transducer.position) < transducerRadius) {
                        isDragging = true;
                        draggingTransducer = true;
                        dragOffset = transducer.position - mouseWorld;
                        std::cout << "Dragging transducer" << std::endl;
                    } else {
                        // Check if clicking on geometry
                        for (size_t i = 0; i < geometries.size(); i++) {
                            if (geometries[i]->contains(mouseWorld)) {
                                isDragging = true;
                                draggingGeometryIndex = (int)i;
                                dragOffset = geometries[i]->position - mouseWorld;
                                std::cout << "Dragging geometry " << i << std::endl;
                                break;
                            }
                        }
                    }
                }
                else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    // Start panning with middle mouse button
                    isPanning = true;
                    panStart = Vec2(event.button.x, event.button.y);
                    viewOffsetStart = renderer.getViewOffset();
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    isDragging = false;
                    draggingTransducer = false;
                    // Remember last dragged geometry for rotation
                    if (draggingGeometryIndex >= 0) {
                        lastDraggedGeometryIndex = draggingGeometryIndex;
                    }
                    draggingGeometryIndex = -1;
                }
                else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    isPanning = false;
                }
            }
            else if (event.type == SDL_MOUSEMOTION) {
                if (isPanning) {
                    // Pan the view with middle mouse button
                    Vec2 mouseCurrent(event.motion.x, event.motion.y);
                    Vec2 screenDelta = mouseCurrent - panStart;
                    // Convert screen delta to world delta (invert Y and scale)
                    Vec2 worldDelta(-screenDelta.x / renderer.getViewScale(),
                                    screenDelta.y / renderer.getViewScale());
                    renderer.setViewOffset(viewOffsetStart + worldDelta);
                }
                else if (isDragging) {
                    // Get mouse position in world coordinates
                    Vec2 mouseScreen(event.motion.x, event.motion.y);
                    Vec2 mouseWorld = renderer.screenToWorld(mouseScreen, currentWidth, currentHeight);

                    if (draggingTransducer) {
                        transducer.position = mouseWorld + dragOffset;

                        // Try to snap to nearest surface
                        // Preserve rotation if already snapped (allows sliding probe along surface)
                        bool snapped = snapping.snapTransducerToSurface(transducer, geometries, transducerSnapped);
                        transducerSnapped = snapped;

                        // Auto-run simulation if enabled
                        if (autoSimulate) {
                            rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                            ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                            simulationRun = true;
                        }
                    } else if (draggingGeometryIndex >= 0) {
                        geometries[draggingGeometryIndex]->position = mouseWorld + dragOffset;

                        // Snap to other geometry
                        if (snapping.snapEnabled) {
                            Vec2 snappedPos = snapping.snapPointToGeometry(
                                geometries[draggingGeometryIndex]->position,
                                geometries[draggingGeometryIndex].get(),
                                geometries
                            );
                            geometries[draggingGeometryIndex]->position = snappedPos;
                        }

                        // Auto-run simulation if enabled
                        if (autoSimulate) {
                            rayTracer.traceFromTransducer(transducer, geometries, numRays, beamSpreadAngle);
                            ascan.generateFromRayPaths(rayTracer.tracedPaths, transducer);
                            simulationRun = true;
                        }
                    }
                }
            }
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    currentWidth = event.window.data1;
                    currentHeight = event.window.data2;
                }
            }
            else if (event.type == SDL_MOUSEWHEEL) {
                // Zoom in/out with mouse wheel
                if (event.wheel.y > 0) {
                    // Scroll up = zoom in
                    renderer.zoom(1.1);
                    std::cout << "Zoom: " << renderer.getViewScale() << "x" << std::endl;
                } else if (event.wheel.y < 0) {
                    // Scroll down = zoom out
                    renderer.zoom(0.9);
                    std::cout << "Zoom: " << renderer.getViewScale() << "x" << std::endl;
                }
            }
        }

        // Clear screen
        renderer.clear();

        // Draw grid
        renderer.drawGrid(currentWidth, currentHeight, 10.0);

        // Draw all geometry objects
        for (const auto& geom : geometries) {
            renderer.drawGeometry(*geom, currentWidth, currentHeight);
        }

        // Draw traced rays (if simulation has been run)
        if (simulationRun) {
            for (const auto& segment : rayTracer.tracedPaths) {
                renderer.drawRaySegment(segment, currentWidth, currentHeight);
            }
        }

        // Draw transducer (on top of rays)
        renderer.drawTransducer(transducer, currentWidth, currentHeight, beamSpreadAngle, transducerSnapped);

        // Draw A-scan panel (right side of screen)
        int ascanWidth = currentWidth / 3;  // 1/3 of screen
        int ascanX = currentWidth - ascanWidth;
        int ascanY = 20;
        int ascanHeight = currentHeight / 2;
        if (simulationRun) {
            renderer.drawAScan(ascan, ascanX, ascanY, ascanWidth, ascanHeight);
        }

        // Draw parameter display (bottom-left corner)
        Color textBg = Color(30, 30, 35, 200);
        SDL_SetRenderDrawColor(sdlRenderer, textBg.r, textBg.g, textBg.b, textBg.a);
        SDL_Rect paramBox = {10, currentHeight - 150, 250, 140};
        SDL_RenderFillRect(sdlRenderer, &paramBox);

        // Draw border
        Color borderCol = Color(100, 100, 100, 255);
        SDL_SetRenderDrawColor(sdlRenderer, borderCol.r, borderCol.g, borderCol.b, borderCol.a);
        SDL_RenderDrawRect(sdlRenderer, &paramBox);

        // Draw parameter text
        Color textCol = Color(220, 220, 220, 255);
        int textX = 20;
        int textY = currentHeight - 145;
        int lineHeight = 18;

        renderer.drawText("PROBE & BEAM PARAMETERS", textX, textY, Color(150, 200, 255, 255));
        textY += lineHeight + 3;

        renderer.drawText("Rays: " + std::to_string(numRays), textX, textY, textCol);
        textY += lineHeight;

        renderer.drawText("Beam: +/-" + std::to_string((int)beamSpreadAngle) + " deg", textX, textY, textCol);
        textY += lineHeight;

        char ampBuf[32];
        std::snprintf(ampBuf, sizeof(ampBuf), "Threshold: %.0f%%", rayTracer.amplitudeThreshold * 100);
        renderer.drawText(ampBuf, textX, textY, textCol);
        textY += lineHeight;

        char rangeBuf[32];
        std::snprintf(rangeBuf, sizeof(rangeBuf), "A-scan: %.0f us", ascan.range);
        renderer.drawText(rangeBuf, textX, textY, textCol);
        textY += lineHeight;

        renderer.drawText("Freq: " + std::to_string((int)transducer.frequency) + " MHz", textX, textY, textCol);
        textY += lineHeight;

        renderer.drawText(std::string("Snap: ") + (snapping.snapEnabled ? "ON" : "OFF"), textX, textY, textCol);

        // Present
        renderer.present();
    }

    // Cleanup
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    std::cout << "BeamCast UT Simulator shut down cleanly" << std::endl;
    return EXIT_SUCCESS;
}
