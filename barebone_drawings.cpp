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

#include "raylib.h"
#include "rlgl.h"

#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <atomic>
#include <limits>
#include <mutex>

// ============================
// SETTINGS HERE (planned: user be able to change in terminal minor update)
// ============================

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
// KEY BINDS 0-9
// ============================

struct BrushBind {

    bool enabled = false;

    bool leftSide = true;


    bool changeR = false;
    bool changeG = false;
    bool changeB = false;
    bool changeA = false;
    bool changeSize = false;


    int r = 0;
    int g = 0;
    int b = 0;
    int a = 255;

    int size = 10;
};


BrushBind binds[10];

std::mutex bindMutex;

// ============================
// HELP
// ============================

void printHelp() {
    std::cout
        << "Commands:\n"
        << " Retrieve info:\n"
        << "  get size <left/right>\n"
        << "  get left\n"
        << "  get right\n"
        << "\n Change settings:\n"
        << "  set size <left/right> <number>\n"
        << "  set left <R> <G> <B> <A>\n"
        << "  set right <R> <G> <B> <A>\n"
        << "\n Canvas controls:\n"
        << "  1 + R/G/B/A + mouse wheel = left RGBA\n"
        << "  2 + R/G/B/A + mouse wheel = right RGBA\n"
        << "  1 + S + mouse wheel = left brush size\n"
        << "  2 + S + mouse wheel = right brush size\n"
        << "  Ctrl + S = save drawing\n"
        << "  Ctrl + O = open drawing\n"
        << "\n Key binds:\n"
        << "  bind <0-9> <left/right> <R/~> <G/~> <B/~> <A/~> <SIZE/~>\n"
        << "  get bind <0-9>\n"
        << "  unbind <0-9>\n"
        << "  Press 0-9 alone to activate a bind\n"
        << "  ~ means leave that setting unchanged\n"
        << "  Using ~ for all five values also clears that bind\n"
        << "\n Brush size warning:\n"
        << "  There is NO artificial brush-size upper limit.\n"
        << "  Brush sizes use a signed 32-bit int and will break/wrap\n"
        << "  past 2147483647 or -2147483648.\n"
        << "\n Type 'help' in the terminal to view this message again.\n\n";
}

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
// RGBA LIMIT FUNCTION
// ============================

void limitRGBA(
    std::atomic<int>& r,
    std::atomic<int>& g,
    std::atomic<int>& b,
    std::atomic<int>& a
) {
    limitAtomic(r, 0, 255);
    limitAtomic(g, 0, 255);
    limitAtomic(b, 0, 255);
    limitAtomic(a, 0, 255);
}

// ============================
// COLOR HELPERS
// ============================

Color opaqueColor(
    std::atomic<int>& r,
    std::atomic<int>& g,
    std::atomic<int>& b
) {
    return {
        (unsigned char)r.load(),
        (unsigned char)g.load(),
        (unsigned char)b.load(),
        255
    };
}

Color alphaStrength(
    int alpha
) {
    return {
        255,
        255,
        255,
        (unsigned char)alpha
    };
}

// ============================
// BRUSH SIZE WARN
// ============================

void adjustBrushSize(
    std::atomic<int>& brushSize,
    int change,
    const char* side
) {
    if (change == 0) {
        return;
    }

    int current = brushSize.load();

    long long attempted =
        (long long)current +
        (long long)change;

    if (
        attempted > std::numeric_limits<int>::max() ||
        attempted < std::numeric_limits<int>::min()
    ) {
        std::cout
            << "\nWARNING: "
            << side
            << " brush size crossed the 32-bit int range "
            << "[-2147483648, 2147483647].\n"
            << "The value will wrap and the brush may break.\n> ";
    }

    brushSize.fetch_add(change);
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
// WINDOWS OPEN
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
// WINDOWS SAVE
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

    int canvasWidth =
        canvas.texture.width;

    int canvasHeight =
        canvas.texture.height;

    // CALCULATE SCALE

    float scaleX =
        (float)canvasWidth /
        imported.width;

    float scaleY =
        (float)canvasHeight /
        imported.height;

    float scale =
        (scaleX < scaleY)
        ? scaleX
        : scaleY;

    // FINAL DRAW SIZE

    float drawWidth =
        imported.width *
        scale;

    float drawHeight =
        imported.height *
        scale;

    // CENTER IMAGE

    float x =
        (
            canvasWidth -
            drawWidth
        ) / 2.0f;

    float y =
        (
            canvasHeight -
            drawHeight
        ) / 2.0f;

    // DRAW INTO CANVAS

    BeginTextureMode(
        canvas
    );

    ClearBackground(
        BACKGROUND_COLOR
    );

    DrawTexturePro(
        imported,
        {
            0,
            0,
            (float)imported.width,
            (float)imported.height
        },
        {
            x,
            y,
            drawWidth,
            drawHeight
        },
        {0, 0},
        0.0f,
        WHITE
    );

    EndTextureMode();

    UnloadTexture(
        imported
    );
}

// ============================
// COLOR BLEND
// ============================

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
// BIND VALUE PARSER
// ============================

bool parseBindValue(
    const std::string& text,
    bool& shouldChange,
    int& value,
    int minimum,
    int maximum
) {

    if (text == "~") {

        shouldChange = false;

        return true;
    }


    try {

        size_t used = 0;

        long long parsed =
            std::stoll(
                text,
                &used
            );


        if (used != text.size()) {
            return false;
        }


        if (
            parsed < minimum ||
            parsed > maximum
        ) {
            return false;
        }


        value =
            (int)parsed;

        shouldChange = true;

        return true;
    }


    catch (...) {

        return false;
    }
}


// ============================
// APPLY BIND
// ============================

void applyBind(
    int number
) {

    BrushBind bind;


    {
        std::lock_guard<std::mutex>
            lock(bindMutex);

        bind =
            binds[number];
    }


    if (!bind.enabled) {
        return;
    }


    if (bind.leftSide) {

        if (bind.changeR) {
            redleft.store(bind.r);
        }

        if (bind.changeG) {
            greenleft.store(bind.g);
        }

        if (bind.changeB) {
            blueleft.store(bind.b);
        }

        if (bind.changeA) {
            alphaleft.store(bind.a);
        }

        if (bind.changeSize) {
            brushSizeLeft.store(
                bind.size
            );
        }
    }


    else {

        if (bind.changeR) {
            redright.store(bind.r);
        }

        if (bind.changeG) {
            greenright.store(bind.g);
        }

        if (bind.changeB) {
            blueright.store(bind.b);
        }

        if (bind.changeA) {
            alpharight.store(bind.a);
        }

        if (bind.changeSize) {
            brushSizeRight.store(
                bind.size
            );
        }
    }
}


// ============================
// TERMINAL THREAD
// ============================

void terminal() {
    while (true) {
        std::string command;

        std::cout << "> ";

        if (
            !std::getline(
                std::cin,
                command
            )
        ) {
            return;
        }

        std::stringstream parser(command);
        std::string action;
        parser >> action;

        // ============================
        // GET
        // ============================

        if (action == "get") {
            std::string target;
            parser >> target;


            // GET BIND

            if (target == "bind") {

                std::string keyText;

                parser >> keyText;


                if (
                    keyText.size() == 1 &&
                    keyText[0] >= '0' &&
                    keyText[0] <= '9'
                ) {

                    int number =
                        keyText[0] - '0';


                    BrushBind bind;


                    {
                        std::lock_guard<std::mutex>
                            lock(bindMutex);

                        bind =
                            binds[number];
                    }


                    if (!bind.enabled) {

                        std::cout
                            << "Bind "
                            << number
                            << " is empty.\n";
                    }


                    else {

                        std::cout
                            << "Bind "
                            << number
                            << " "
                            << (
                                bind.leftSide
                                ? "left "
                                : "right "
                            );


                        if (bind.changeR) {
                            std::cout << bind.r;
                        }
                        else {
                            std::cout << "~";
                        }

                        std::cout << " ";


                        if (bind.changeG) {
                            std::cout << bind.g;
                        }
                        else {
                            std::cout << "~";
                        }

                        std::cout << " ";


                        if (bind.changeB) {
                            std::cout << bind.b;
                        }
                        else {
                            std::cout << "~";
                        }

                        std::cout << " ";


                        if (bind.changeA) {
                            std::cout << bind.a;
                        }
                        else {
                            std::cout << "~";
                        }

                        std::cout << " ";


                        if (bind.changeSize) {
                            std::cout << bind.size;
                        }
                        else {
                            std::cout << "~";
                        }


                        std::cout << "\n";
                    }
                }


                else {

                    std::cout
                        << "FAILED; Usage: get bind <0-9>\n";
                }
            }


            // ============================
            // GET SIZE
            // ============================

            else if (target == "size") {
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
                        << "FAILED; Usage: get size <left/right>\n";
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
                    << "FAILED; Unknown get target: "
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

            // SET SIZE

            if (target == "size") {
                std::string side;
                long long requestedSize;

                parser >> side;

                if (parser >> requestedSize) {
                    if (
                        requestedSize > std::numeric_limits<int>::max() ||
                        requestedSize < std::numeric_limits<int>::min()
                    ) {
                        std::cout
                            << "WARNING: brush sizes use a signed 32-bit int.\n"
                            << "Values past 2147483647 or -2147483648 "
                            << "cannot be represented safely by this setting command.\n"
                            << "Brush size was not changed.\n";
                    }

                    else {
                        int newSize =
                            (int)requestedSize;

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
                                << "FAILED; Usage: set size <left/right> <number>\n";
                        }
                    }
                }

                else {
                    std::cout
                        << "FAILED; Usage: set size <left/right> <number>\n";
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
                            << "FAILED; RGBA must be between 0 and 255.\n";
                    }
                }

                else {
                    std::cout
                        << "FAILED; Usage: set left <R> <G> <B> <A>\n";
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
                            << "FAILED; RGBA must be between 0 and 255.\n";
                    }
                }

                else {
                    std::cout
                        << "FAILED; Usage: set right <R> <G> <B> <A>\n";
                }
            }

            else {
                std::cout
                    << "FAILED; Unknown set target: "
                    << target
                    << "\n";
            }
        }

        // ============================
        // CREATE / REPLACE BIND
        // ============================

        else if (action == "bind") {

            std::string keyText;
            std::string side;

            std::string rText;
            std::string gText;
            std::string bText;
            std::string aText;
            std::string sizeText;


            if (
                parser
                >> keyText
                >> side
                >> rText
                >> gText
                >> bText
                >> aText
                >> sizeText
            ) {

                if (
                    keyText.size() != 1 ||
                    keyText[0] < '0' ||
                    keyText[0] > '9'
                ) {

                    std::cout
                        << "FAILED; key must be 0-9.\n";

                    continue;
                }


                if (
                    side != "left" &&
                    side != "right"
                ) {

                    std::cout
                        << "FAILED; side must be left or right.\n";

                    continue;
                }


                int number =
                    keyText[0] - '0';


                BrushBind newBind;

                newBind.enabled = true;

                newBind.leftSide =
                    (side == "left");


                bool good = true;


                good &=
                    parseBindValue(
                        rText,
                        newBind.changeR,
                        newBind.r,
                        0,
                        255
                    );


                good &=
                    parseBindValue(
                        gText,
                        newBind.changeG,
                        newBind.g,
                        0,
                        255
                    );


                good &=
                    parseBindValue(
                        bText,
                        newBind.changeB,
                        newBind.b,
                        0,
                        255
                    );


                good &=
                    parseBindValue(
                        aText,
                        newBind.changeA,
                        newBind.a,
                        0,
                        255
                    );


                good &=
                    parseBindValue(
                        sizeText,
                        newBind.changeSize,
                        newBind.size,
                        1,
                        std::numeric_limits<int>::max()
                    );


                if (!good) {

                    std::cout
                        << "FAILED; RGBA must be 0-255, "
                        << "size must be 1-2147483647, "
                        << "or use ~.\n";

                    continue;
                }


                bool changesAnything =

                    newBind.changeR ||
                    newBind.changeG ||
                    newBind.changeB ||
                    newBind.changeA ||
                    newBind.changeSize;


                // All five "~" means manual unbind.

                if (!changesAnything) {

                    {
                        std::lock_guard<std::mutex>
                            lock(bindMutex);

                        binds[number] =
                            BrushBind{};
                    }


                    std::cout
                        << "Bind "
                        << number
                        << " removed because all five values were ~.\n";

                    continue;
                }


                {
                    std::lock_guard<std::mutex>
                        lock(bindMutex);

                    binds[number] =
                        newBind;
                }


                std::cout
                    << "Bind "
                    << number
                    << " created for "
                    << side
                    << " brush.\n";
            }


            else {

                std::cout
                    << "FAILED; Usage:\n"
                    << "bind <0-9> <left/right> "
                    << "<R/~> <G/~> <B/~> <A/~> <SIZE/~>\n";
            }
        }


        // ============================
        // DELETE BIND
        // ============================

        else if (action == "unbind") {

            std::string keyText;

            parser >> keyText;


            if (
                keyText.size() == 1 &&
                keyText[0] >= '0' &&
                keyText[0] <= '9'
            ) {

                int number =
                    keyText[0] - '0';


                {
                    std::lock_guard<std::mutex>
                        lock(bindMutex);

                    binds[number] =
                        BrushBind{};
                }


                std::cout
                    << "Bind "
                    << number
                    << " removed.\n";
            }


            else {

                std::cout
                    << "FAILED; Usage: unbind <0-9>\n";
            }
        }


        // HELP

        else if (action == "help") {
            printHelp();
        }

        // EMPTY

        else if (action.empty()) {
            // Do nothing for an empty line.
        }

        // UNKNOWN

        else {
            std::cout
                << "FAILED; Unknown command: \""
                << command
                << "\"\n";
        }
    }
}

// ============================
// MAIN
// ============================

int main() {
    // ============================
    // CANVAS SIZE
    // ============================

    int screenWidth;
    int screenHeight;

    std::cout
        << "Barebone Drawings v0.2.0\n\n";

    std::cout
        << "Enter desired canvas width: ";

    while (
        !(std::cin >> screenWidth) ||
        screenWidth <= 0
    ) {
        std::cout
            << "FAILED; width must be a positive integer.\n"
            << "Enter desired canvas width: ";

        std::cin.clear();

        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(),
            '\n'
        );
    }

    std::cout
        << "Enter desired canvas height: ";

    while (
        !(std::cin >> screenHeight) ||
        screenHeight <= 0
    ) {
        std::cout
            << "FAILED; height must be a positive integer.\n"
            << "Enter desired canvas height: ";

        std::cin.clear();

        std::cin.ignore(
            std::numeric_limits<std::streamsize>::max(),
            '\n'
        );
    }


    std::cin.ignore(
        std::numeric_limits<std::streamsize>::max(),
        '\n'
    );

    // ============================
    // START TERMINAL COMMAND THREAD
    // ============================

    printHelp();

    std::thread terminalThread(
        terminal
    );

    terminalThread.detach();

    // ============================
    // CREATE DRAWING WINDOW
    // ============================

    InitWindow(
        screenWidth,
        screenHeight,
        "Barebone Drawings v0.2.0"
    );

    SetTargetFPS(
        TARGET_FPS
    );

    // ============================
    // PERMANENT CANVAS
    // ============================

    RenderTexture2D canvas =
        LoadRenderTexture(
            screenWidth,
            screenHeight
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
            screenWidth,
            screenHeight
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
            screenWidth,
            screenHeight
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
            (
                IsKeyDown(KEY_LEFT_CONTROL) ||
                IsKeyDown(KEY_RIGHT_CONTROL)
            ) &&
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
            (
                IsKeyDown(KEY_LEFT_CONTROL) ||
                IsKeyDown(KEY_RIGHT_CONTROL)
            ) &&
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
        // USE KEY BINDS
        // ============================

        bool changingSetting =

            IsKeyDown(KEY_R) ||
            IsKeyDown(KEY_G) ||
            IsKeyDown(KEY_B) ||
            IsKeyDown(KEY_A) ||
            IsKeyDown(KEY_S);


        if (!changingSetting) {

            if (IsKeyPressed(KEY_ONE)) {
                applyBind(1);
            }

            if (IsKeyPressed(KEY_TWO)) {
                applyBind(2);
            }

            if (IsKeyPressed(KEY_THREE)) {
                applyBind(3);
            }

            if (IsKeyPressed(KEY_FOUR)) {
                applyBind(4);
            }

            if (IsKeyPressed(KEY_FIVE)) {
                applyBind(5);
            }

            if (IsKeyPressed(KEY_SIX)) {
                applyBind(6);
            }

            if (IsKeyPressed(KEY_SEVEN)) {
                applyBind(7);
            }

            if (IsKeyPressed(KEY_EIGHT)) {
                applyBind(8);
            }

            if (IsKeyPressed(KEY_NINE)) {
                applyBind(9);
            }

            if (IsKeyPressed(KEY_ZERO)) {
                applyBind(0);
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
        // BRUSH SIZE CONTROLS
        // ============================

        if (
            IsKeyDown(KEY_S) &&
            IsKeyDown(KEY_ONE)
        ) {
            adjustBrushSize(
                brushSizeLeft,
                wheel,
                "left"
            );
        }

        if (
            IsKeyDown(KEY_S) &&
            IsKeyDown(KEY_TWO)
        ) {
            adjustBrushSize(
                brushSizeRight,
                wheel,
                "right"
            );
        }

        // ============================
        // LIMIT RGBA VALUES
        // ============================

        limitRGBA(
            redleft,
            greenleft,
            blueleft,
            alphaleft
        );

        limitRGBA(
            redright,
            greenright,
            blueright,
            alpharight
        );

        // ============================
        // READ VALUES NEEDED THIS FRAME
        // ============================

        int currentBrushSizeLeft =
            brushSizeLeft.load();

        int currentBrushSizeRight =
            brushSizeRight.load();

        int currentAlphaLeft =
            alphaleft.load();

        int currentAlphaRight =
            alpharight.load();

        Color leftStrokeColor =
            opaqueColor(
                redleft,
                greenleft,
                blueleft
            );

        Color rightStrokeColor =
            opaqueColor(
                redright,
                greenright,
                blueright
            );

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
                currentBrushSizeLeft * 2.0f,
                leftStrokeColor
            );

            DrawCircleV(
                mouse,
                (float)currentBrushSizeLeft,
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
            Color leftStrength =
                alphaStrength(
                    activeLeftAlpha
                );

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
                currentBrushSizeRight * 2.0f,
                rightStrokeColor
            );

            DrawCircleV(
                mouse,
                (float)currentBrushSizeRight,
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
            Color rightStrength =
                alphaStrength(
                    activeRightAlpha
                );

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

            BeginTextureMode(
                rightStroke
            );

            ClearBackground(
                BLANK
            );

            EndTextureMode();
        }

        // DISPLAY

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
            Color leftPreview =
                alphaStrength(
                    activeLeftAlpha
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
            Color rightPreview =
                alphaStrength(
                    activeRightAlpha
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