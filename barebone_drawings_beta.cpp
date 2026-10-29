// ============================
// WINDOWS
// ============================

// Rename Windows functions that conflict with raylib
#define CloseWindow CloseWindowWin32
#define Rectangle RectangleWin32
#define ShowCursor ShowCursorWin32

#define DrawTextA DrawTextAWin32
#define DrawTextW DrawTextWWin32

#define DrawTextExA DrawTextExAWin32
#define DrawTextExW DrawTextExWWin32

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>


// Remove Windows macros before including raylib
#undef CloseWindow
#undef Rectangle
#undef ShowCursor

#undef LoadImage

#undef DrawText
#undef DrawTextA
#undef DrawTextW

#undef DrawTextEx
#undef DrawTextExA
#undef DrawTextExW


// ============================
// RAYLIB
// ============================

#include "raylib.h"
#include "rlgl.h"


// ============================
// C++
// ============================

#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <atomic>
#include <limits>


// ============================
// EDIT SETTINGS HERE
// ============================

const int SCREEN_WIDTH = 1500;
const int SCREEN_HEIGHT = 900;

const int TARGET_FPS = 60;

const Color BACKGROUND_COLOR = {
    255,
    255,
    255,
    255
};


// ============================
// SHARED VARIABLES
// ============================

std::atomic<int> brushSizeLeft = 10;
std::atomic<int> brushSizeRight = 10;

std::atomic<int> redleft = 0;
std::atomic<int> greenleft = 0;
std::atomic<int> blueleft = 0;
std::atomic<int> alphaleft = 255;

std::atomic<int> redright = 255;
std::atomic<int> greenright = 255;
std::atomic<int> blueright = 255;
std::atomic<int> alpharight = 255;


// ============================
// ATOMIC LIMIT FUNCTION
// ============================

void limitAtomic(
    std::atomic<int>& value,
    int minimum,
    int maximum
) {
    int current = value.load();

    while (true) {

        int limited = current;

        if (limited < minimum) {
            limited = minimum;
        }

        if (limited > maximum) {
            limited = maximum;
        }

        if (limited == current) {
            return;
        }

        if (
            value.compare_exchange_weak(
                current,
                limited
            )
        ) {
            return;
        }
    }
}

// ============================
// SAVE DRAWING
// ============================

void saveCanvas(
    RenderTexture2D& canvas,
    const std::string& filename
) {

    Image image =
        LoadImageFromTexture(
            canvas.texture
        );

    ImageFlipVertical(
        &image
    );

    ExportImage(
        image,
        filename.c_str()
    );

    UnloadImage(
        image
    );
}

// ============================
// WINDOWS SAVE
// ============================

std::string pickOpenFile() {

    char filename[MAX_PATH] = "";

    OPENFILENAMEA dialog = {};

    dialog.lStructSize = sizeof(dialog);

    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;

    dialog.lpstrFilter =
        "PNG Images\0*.png\0"
        "All Files\0*.*\0";

    dialog.nFilterIndex = 1;

    dialog.Flags =
        OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&dialog)) {
        return filename;
    }

    return "";
}

// ============================
// WINDOWS LOAD
// ============================

std::string pickSaveFile() {

    char filename[MAX_PATH] = "drawing.png";

    OPENFILENAMEA dialog = {};

    dialog.lStructSize = sizeof(dialog);

    dialog.lpstrFile = filename;
    dialog.nMaxFile = MAX_PATH;

    dialog.lpstrFilter =
        "PNG Images\0*.png\0"
        "All Files\0*.*\0";

    dialog.nFilterIndex = 1;

    dialog.lpstrDefExt = "png";

    dialog.Flags =
        OFN_PATHMUSTEXIST |
        OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&dialog)) {
        return filename;
    }

    return "";
}

// ============================
// LOAD DRAWING
// ============================

void loadCanvas(
    RenderTexture2D& canvas,
    const std::string& filename
) {

    Texture2D imported =
        LoadTexture(
            filename.c_str()
        );


    if (imported.id == 0) {

        std::cout
            << "Could not load image.\n";

        return;
    }


    // ============================
    // CALCULATE SCALE
    // ============================

    float scaleX =
        (float)SCREEN_WIDTH /
        imported.width;

    float scaleY =
        (float)SCREEN_HEIGHT /
        imported.height;


    float scale =
        (scaleX < scaleY)
        ? scaleX
        : scaleY;


    // ============================
    // FINAL DRAW SIZE
    // ============================

    float drawWidth =
        imported.width *
        scale;

    float drawHeight =
        imported.height *
        scale;


    // ============================
    // CENTER IMAGE
    // ============================

    float x =
        (
            SCREEN_WIDTH -
            drawWidth
        ) / 2.0f;


    float y =
        (
            SCREEN_HEIGHT -
            drawHeight
        ) / 2.0f;


    // ============================
    // DRAW INTO CANVAS
    // ============================

    BeginTextureMode(
        canvas
    );


    ClearBackground(
        BACKGROUND_COLOR
    );


    DrawTexturePro(
        imported,

        // source rectangle
        {
            0,
            0,
            (float)imported.width,
            (float)imported.height
        },

        // destination rectangle
        {
            x,
            y,
            drawWidth,
            drawHeight
        },

        // rotation origin
        {0, 0},

        // rotation
        0.0f,

        // tint
        WHITE
    );


    EndTextureMode();


    UnloadTexture(
        imported
    );
}

// ============================
// CUSTOM COLOR BLEND
// ============================
//
// RGB:
// result = source * alpha
//        + destination * (1 - alpha)
//
// Alpha:
// keep destination alpha unchanged
//
// This means your "alpha" acts like
// paint strength / overwrite strength.
//

void beginColorBlend() {

    rlSetBlendFactorsSeparate(
        RL_SRC_ALPHA,
        RL_ONE_MINUS_SRC_ALPHA,

        RL_ZERO,
        RL_ONE,

        RL_FUNC_ADD,
        RL_FUNC_ADD
    );

    BeginBlendMode(
        BLEND_CUSTOM_SEPARATE
    );
}


// ============================
// TERMINAL THREAD
// ============================

void terminal() {

    while (true) {

        std::string command;

        std::cout << "> ";

        std::getline(
            std::cin,
            command
        );


        std::stringstream parser(command);

        std::string action;

        parser >> action;


        // ============================
        // GET
        // ============================

        if (action == "get") {

            std::string target;

            parser >> target;


            if (target == "size") {

            std::string side;
            parser >> side;

            if (side == "left") {
                std::cout
                    << "Left brush size: "
                    << brushSizeLeft.load()
                    << "\n";
            }

            else if (side == "right") {
                std::cout
                    << "Right brush size: "
                    << brushSizeRight.load()
                    << "\n";
            }

            else {
                std::cout
                    << "Usage: get size <left/right>\n";
                }
            }


            else if (target == "left") {

                std::cout
                    << "Left RGBA: "
                    << redleft.load() << " "
                    << greenleft.load() << " "
                    << blueleft.load() << " "
                    << alphaleft.load()
                    << "\n";
            }


            else if (target == "right") {

                std::cout
                    << "Right RGBA: "
                    << redright.load() << " "
                    << greenright.load() << " "
                    << blueright.load() << " "
                    << alpharight.load()
                    << "\n";
            }


            else {

                std::cout
                    << "Unknown get target: "
                    << target
                    << "\n";
            }
        }


        // ============================
        // SET
        // ============================

        else if (action == "set") {

            std::string target;

            parser >> target;


            // ============================
            // SET SIZE
            // ============================

            if (target == "size") {

                std::string side;
                int newSize;

                parser >> side;

                if (parser >> newSize) {

                    if (newSize < 1) {
                        newSize = 1;
                    }


                    if (side == "left") {

                        brushSizeLeft.store(
                            newSize
                        );

                        std::cout
                            << "Brush size left set to "
                            << newSize
                            << "\n";
                    }


                    else if (side == "right") {

                        brushSizeRight.store(
                            newSize
                        );

                        std::cout
                            << "Brush size right set to "
                            << newSize
                            << "\n";
                    }


                    else {

                        std::cout
                            << "Usage: set size <left/right> <number>\n";
                    }
                }


                else {

                    std::cout
                        << "Usage: set size <left/right> <number>\n";
                }
            }

            // ============================
            // SET LEFT
            // ============================

            else if (target == "left") {

                int r;
                int g;
                int b;
                int a;

                if (
                    parser
                    >> r
                    >> g
                    >> b
                    >> a
                ) {

                    if (
                        r >= 0 && r <= 255 &&
                        g >= 0 && g <= 255 &&
                        b >= 0 && b <= 255 &&
                        a >= 0 && a <= 255
                    ) {

                        redleft.store(r);
                        greenleft.store(g);
                        blueleft.store(b);
                        alphaleft.store(a);

                        std::cout
                            << "Left RGBA set to "
                            << r << " "
                            << g << " "
                            << b << " "
                            << a
                            << "\n";
                    }

                    else {

                        std::cout
                            << "RGBA must be between 0 and 255.\n";
                    }
                }

                else {

                    std::cout
                        << "Usage: set left <R> <G> <B> <A>\n";
                }
            }


            // ============================
            // SET RIGHT
            // ============================

            else if (target == "right") {

                int r;
                int g;
                int b;
                int a;

                if (
                    parser
                    >> r
                    >> g
                    >> b
                    >> a
                ) {

                    if (
                        r >= 0 && r <= 255 &&
                        g >= 0 && g <= 255 &&
                        b >= 0 && b <= 255 &&
                        a >= 0 && a <= 255
                    ) {

                        redright.store(r);
                        greenright.store(g);
                        blueright.store(b);
                        alpharight.store(a);

                        std::cout
                            << "Right RGBA set to "
                            << r << " "
                            << g << " "
                            << b << " "
                            << a
                            << "\n";
                    }

                    else {

                        std::cout
                            << "RGBA must be between 0 and 255.\n";
                    }
                }

                else {

                    std::cout
                        << "Usage: set right <R> <G> <B> <A>\n";
                }
            }


            else {

                std::cout
                    << "Unknown set target: "
                    << target
                    << "\n";
            }
        }


        // ============================
        // HELP
        // ============================

        else if (action == "help") {

            std::cout
                << "Commands:\n"
                << "  get size <left/right> \n"
                << "  get left\n"
                << "  get right\n"
                << "  set size <left/right> <number>\n"
                << "  set left <R> <G> <B> <A>\n"
                << "  set right <R> <G> <B> <A>\n";
        }


        // ============================
        // UNKNOWN
        // ============================

        else {

            std::cout
                << "Unknown command: \""
                << command
                << "\"\n";
        }
    }
}


// ============================
// MAIN
// ============================

int main() {

    std::thread terminalThread(
        terminal
    );

    terminalThread.detach();


    InitWindow(
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        "Barebone Drawings Beta :3"
    );


    SetTargetFPS(
        TARGET_FPS
    );


    // ============================
    // PERMANENT CANVAS
    // ============================

    RenderTexture2D canvas =
        LoadRenderTexture(
            SCREEN_WIDTH,
            SCREEN_HEIGHT
        );


    BeginTextureMode(
        canvas
    );

    ClearBackground(
        BACKGROUND_COLOR
    );

    EndTextureMode();


    // ============================
    // TEMPORARY LEFT STROKE
    // ============================

    RenderTexture2D leftStroke =
        LoadRenderTexture(
            SCREEN_WIDTH,
            SCREEN_HEIGHT
        );


    BeginTextureMode(
        leftStroke
    );

    ClearBackground(
        BLANK
    );

    EndTextureMode();


    // ============================
    // TEMPORARY RIGHT STROKE
    // ============================

    RenderTexture2D rightStroke =
        LoadRenderTexture(
            SCREEN_WIDTH,
            SCREEN_HEIGHT
        );


    BeginTextureMode(
        rightStroke
    );

    ClearBackground(
        BLANK
    );

    EndTextureMode();


    // ============================
    // MOUSE MEMORY
    // ============================

    Vector2 previousMouseLeft =
        GetMousePosition();

    Vector2 previousMouseRight =
        GetMousePosition();


    // ============================
    // ACTIVE STROKE ALPHA
    // ============================

    int activeLeftAlpha = 255;
    int activeRightAlpha = 255;


    // ============================
    // MAIN LOOP
    // ============================

    while (!WindowShouldClose()) {

        Vector2 mouse =
            GetMousePosition();


        int wheel =
            (int)GetMouseWheelMove();

        // ============================
        // LOAD/SAVE
        // ============================        

        if (
            IsKeyDown(KEY_LEFT_CONTROL) &&
            IsKeyPressed(KEY_S)
        ) {

            std::string filename =
                pickSaveFile();

            if (!filename.empty()) {

                saveCanvas(
                    canvas,
                    filename
                );
            }
        }

        if (
            IsKeyDown(KEY_LEFT_CONTROL) &&
            IsKeyPressed(KEY_O)
        ) {

            std::string filename =
                pickOpenFile();

            if (!filename.empty()) {

                loadCanvas(
                    canvas,
                    filename
                );
            }
        }

        // ============================
        // LEFT COLOR CONTROLS
        // ============================

        if (
            IsKeyDown(KEY_R) &&
            IsKeyDown(KEY_ONE)
        ) {

            redleft.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_G) &&
            IsKeyDown(KEY_ONE)
        ) {

            greenleft.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_B) &&
            IsKeyDown(KEY_ONE)
        ) {

            blueleft.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_A) &&
            IsKeyDown(KEY_ONE)
        ) {

            alphaleft.fetch_add(
                wheel
            );
        }


        // ============================
        // RIGHT COLOR CONTROLS
        // ============================

        if (
            IsKeyDown(KEY_R) &&
            IsKeyDown(KEY_TWO)
        ) {

            redright.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_G) &&
            IsKeyDown(KEY_TWO)
        ) {

            greenright.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_B) &&
            IsKeyDown(KEY_TWO)
        ) {

            blueright.fetch_add(
                wheel
            );
        }


        if (
            IsKeyDown(KEY_A) &&
            IsKeyDown(KEY_TWO)
        ) {

            alpharight.fetch_add(
                wheel
            );
        }


        // ============================
        // LIMIT VALUES
        // ============================

        limitAtomic(
            redleft,
            0,
            255
        );

        limitAtomic(
            greenleft,
            0,
            255
        );

        limitAtomic(
            blueleft,
            0,
            255
        );

        limitAtomic(
            alphaleft,
            0,
            255
        );


        limitAtomic(
            redright,
            0,
            255
        );

        limitAtomic(
            greenright,
            0,
            255
        );

        limitAtomic(
            blueright,
            0,
            255
        );

        limitAtomic(
            alpharight,
            0,
            255
        );


        // ============================
        // READ ATOMIC VALUES
        // ============================

        int currentBrushSizeLeft =
            brushSizeLeft.load();

        int currentBrushSizeRight =
            brushSizeRight.load();


        int currentRedLeft =
            redleft.load();

        int currentGreenLeft =
            greenleft.load();

        int currentBlueLeft =
            blueleft.load();

        int currentAlphaLeft =
            alphaleft.load();


        int currentRedRight =
            redright.load();

        int currentGreenRight =
            greenright.load();

        int currentBlueRight =
            blueright.load();

        int currentAlphaRight =
            alpharight.load();


        // ============================F
        // OPAQUE STROKE COLORS
        // ============================

        Color leftStrokeColor = {

            (unsigned char)
                currentRedLeft,

            (unsigned char)
                currentGreenLeft,

            (unsigned char)
                currentBlueLeft,

            255
        };


        Color rightStrokeColor = {

            (unsigned char)
                currentRedRight,

            (unsigned char)
                currentGreenRight,

            (unsigned char)
                currentBlueRight,

            255
        };


        // ============================
        // START LEFT STROKE
        // ============================

        if (
            IsMouseButtonPressed(
                MOUSE_BUTTON_LEFT
            )
        ) {

            previousMouseLeft =
                mouse;


            activeLeftAlpha =
                currentAlphaLeft;


            BeginTextureMode(
                leftStroke
            );

            ClearBackground(
                BLANK
            );

            EndTextureMode();
        }


        // ============================
        // DRAW LEFT STROKE
        // ============================

        if (
            IsMouseButtonDown(
                MOUSE_BUTTON_LEFT
            )
        ) {

            BeginTextureMode(
                leftStroke
            );


            DrawLineEx(
                previousMouseLeft,
                mouse,
                currentBrushSizeLeft * 2,
                leftStrokeColor
            );

            DrawCircleV(
                mouse,
                currentBrushSizeLeft,
                leftStrokeColor
            );


            EndTextureMode();


            previousMouseLeft =
                mouse;
        }


        // ============================
        // COMMIT LEFT STROKE
        // ============================

        if (
            IsMouseButtonReleased(
                MOUSE_BUTTON_LEFT
            )
        ) {

            Color leftStrength = {

                255,
                255,
                255,

                (unsigned char)
                    activeLeftAlpha
            };


            BeginTextureMode(
                canvas
            );


            beginColorBlend();


            DrawTextureRec(
                leftStroke.texture,

                {
                    0,
                    0,

                    (float)
                        leftStroke.texture.width,

                    -(float)
                        leftStroke.texture.height
                },

                {0, 0},

                leftStrength
            );


            EndBlendMode();


            EndTextureMode();


            // Clear temporary stroke

            BeginTextureMode(
                leftStroke
            );

            ClearBackground(
                BLANK
            );

            EndTextureMode();
        }


        // ============================
        // START RIGHT STROKE
        // ============================

        if (
            IsMouseButtonPressed(
                MOUSE_BUTTON_RIGHT
            )
        ) {

            previousMouseRight =
                mouse;


            activeRightAlpha =
                currentAlphaRight;


            BeginTextureMode(
                rightStroke
            );

            ClearBackground(
                BLANK
            );

            EndTextureMode();
        }


        // ============================
        // DRAW RIGHT STROKE
        // ============================

        if (
            IsMouseButtonDown(
                MOUSE_BUTTON_RIGHT
            )
        ) {

            BeginTextureMode(
                rightStroke
            );


            DrawLineEx(
                previousMouseRight,
                mouse,
                currentBrushSizeRight * 2,
                rightStrokeColor
            );

            DrawCircleV(
                mouse,
                currentBrushSizeRight,
                rightStrokeColor
            );


            EndTextureMode();


            previousMouseRight =
                mouse;
        }


        // ============================
        // COMMIT RIGHT STROKE
        // ============================

        if (
            IsMouseButtonReleased(
                MOUSE_BUTTON_RIGHT
            )
        ) {

            Color rightStrength = {

                255,
                255,
                255,

                (unsigned char)
                    activeRightAlpha
            };


            BeginTextureMode(
                canvas
            );


            beginColorBlend();


            DrawTextureRec(
                rightStroke.texture,

                {
                    0,
                    0,

                    (float)
                        rightStroke.texture.width,

                    -(float)
                        rightStroke.texture.height
                },

                {0, 0},

                rightStrength
            );


            EndBlendMode();


            EndTextureMode();


            // Clear temporary stroke

            BeginTextureMode(
                rightStroke
            );

            ClearBackground(
                BLANK
            );

            EndTextureMode();
        }


        // ============================
        // DISPLAY
        // ============================

        BeginDrawing();


        ClearBackground(
            BACKGROUND_COLOR
        );


        // ============================
        // PERMANENT CANVAS
        // ============================

        DrawTextureRec(
            canvas.texture,

            {
                0,
                0,

                (float)
                    canvas.texture.width,

                -(float)
                    canvas.texture.height
            },

            {0, 0},

            WHITE
        );


        // ============================
        // LEFT LIVE PREVIEW
        // ============================

        if (
            IsMouseButtonDown(
                MOUSE_BUTTON_LEFT
            )
        ) {

            Color leftPreview = {

                255,
                255,
                255,

                (unsigned char)
                    activeLeftAlpha
            };


            beginColorBlend();


            DrawTextureRec(
                leftStroke.texture,

                {
                    0,
                    0,

                    (float)
                        leftStroke.texture.width,

                    -(float)
                        leftStroke.texture.height
                },

                {0, 0},

                leftPreview
            );


            EndBlendMode();
        }


        // ============================
        // RIGHT LIVE PREVIEW
        // ============================

        if (
            IsMouseButtonDown(
                MOUSE_BUTTON_RIGHT
            )
        ) {

            Color rightPreview = {

                255,
                255,
                255,

                (unsigned char)
                    activeRightAlpha
            };


            beginColorBlend();


            DrawTextureRec(
                rightStroke.texture,

                {
                    0,
                    0,

                    (float)
                        rightStroke.texture.width,

                    -(float)
                        rightStroke.texture.height
                },

                {0, 0},

                rightPreview
            );


            EndBlendMode();
        }


        EndDrawing();
    }


    // ============================
    // CLEANUP
    // ============================

    UnloadRenderTexture(
        leftStroke
    );

    UnloadRenderTexture(
        rightStroke
    );

    UnloadRenderTexture(
        canvas
    );


    CloseWindow();


    return 0;
}