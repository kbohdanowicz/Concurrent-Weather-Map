#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define KEY 1234

#define CURSOR_ICON_WIDTH 6
#define CURSOR_ICON_HEIGHT 24

#define ASCII_DEGREE 176

// Cursor shape
static char cursoricon_bits[] =
{
        0x21, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
        0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x21
};

#define COORDINATES_COUNT 8

enum
{
    WIN_X = 100,
    WIN_Y = 500,
    WIN_WIDTH = 1200,
    WIN_HEIGHT = 800,
    WIN_BORDER = 1
};

enum{
    NORTH = 0,
    SOUTH = 1,
    EAST = 2,
    WEST = 3
};

typedef struct
{
    Window window;
    int position, current, end;
    char content[5];
}
TextArea;

typedef struct
{
    //useless
    Window window;
    Window rootWindow;

    int x, y;
    unsigned int width, height;
    unsigned int border_width;
    unsigned int depth;

    XFontStruct *font;
    XTextItem textItem[1];
}
Button;

typedef struct
{
    Display *display;
    int screen;
    Visual *visual;
    int depth;
    XSetWindowAttributes windowAttributes;
    unsigned long mask;

    Colormap colormap;
    GC gc;

    XColor color_gray;
    XColor exact, closest;

    Window rootWindow;

    Window window;

    Button button;
    TextArea textArea1;
    TextArea textArea2;
    TextArea textArea3;

    XEvent event;

    XFontStruct *font;

    Drawable drawable;
}
Scene;

typedef struct
{
    char name[17];
    char data1[30];
    char data2[30];
    char data3[30];
}
Station;

typedef struct
{
    Station arr[4];

}
AllStations;

/* Global variables */
Scene scene;

// Color
int red;

int charinc;

TextArea *selectedTextArea;

KeySym character;
XComposeStatus xComposeStatus;
Pixmap cursor;

AllStations *allStations;
int allStationsId;

int currentStationId;

/* FUNCTIONS */

/* NAMING CONVENTIONS: */
//   camelCase - function to use in X or E function
//   X_PascalCase - main function
//   E_PascalCase - function to use in X_EventLoop() function

void setUpScene(){
    scene.display = XOpenDisplay(NULL);
    if (scene.display == NULL)
    {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }
    scene.screen = DefaultScreen(scene.display);
    scene.rootWindow = RootWindow(scene.display, scene.screen);

    //whats this?
    scene.depth = DefaultDepth(scene.display, scene.screen);
    scene.visual = DefaultVisual(scene.display, scene.screen);

    scene.windowAttributes.event_mask = ExposureMask | KeyPressMask | ButtonPressMask;
    scene.mask = CWBackPixel | CWBorderPixel | CWEventMask;
}

void setUpGraphics()
{
    scene.gc = DefaultGC(scene.display, scene.window);
    scene.colormap = DefaultColormap(scene.display, scene.screen);

    // border color
    scene.windowAttributes.border_pixel = BlackPixel(scene.display,scene.screen);
    // background color
    scene.windowAttributes.background_pixel = WhitePixel(scene.display, scene.screen);
}

void setUpColors()
{
    XParseColor(scene.display, scene.colormap, "rgb:cc/cc/cc", &scene.color_gray);
    XAllocColor(scene.display, scene.colormap, &scene.color_gray);
}

void X_CreateScene()
{
    setUpScene();
    setUpGraphics();
    setUpColors();

    scene.window = XCreateWindow(
            scene.display, scene.rootWindow,
            WIN_X, WIN_Y, WIN_WIDTH, WIN_HEIGHT, WIN_BORDER,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &scene.windowAttributes
            );

    XSelectInput(
            scene.display, scene.window,
            scene.windowAttributes.event_mask
            );

    // display the window
    XMapWindow(scene.display, scene.window);

    scene.drawable = scene.window;
}

void X_SetUpFont()
{
    const char *fontName = "7x14";
    scene.font = XLoadQueryFont(scene.display, fontName);

    if (!scene.font)
    {
        fprintf( stderr, "Unable to load font %s: using fixed\n", fontName);
        scene.font = XLoadQueryFont(scene.display, "fixed");
    }

    XSetFont(scene.display, scene.gc, scene.font -> fid); //fid == font id
}

void X_CreateButton()
{
    scene.button.window = XCreateSimpleWindow(
            scene.display, scene.window,
            890, 100, 50, 25, WIN_BORDER,
            BlackPixel(scene.display, scene.screen), scene.color_gray.pixel
    );

    XSelectInput(
            scene.display, scene.button.window,
            ButtonPressMask
    );

    // display the button
    XMapWindow(scene.display, scene.button.window);

    // center text
    XGetGeometry(
            scene.display, scene.button.window, &scene.button.rootWindow,
            &scene.button.x, &scene.button.y, &scene.button.width, &scene.button.height,
            &scene.button.border_width, &scene.button.depth
    );
}

void setUpTextAreaResources()
{
    charinc = scene.font -> per_char -> width;

    XAllocNamedColor(scene.display, scene.colormap, "red", &scene.exact, &scene.closest);
    red = scene.closest.pixel;

    cursor = XCreatePixmapFromBitmapData(
            scene.display, scene.window,
            cursoricon_bits, CURSOR_ICON_WIDTH, CURSOR_ICON_HEIGHT,
            red, WhitePixel(scene.display, scene.screen),
            scene.depth
            );
}

void E_DrawTextAreaLabels()
{
    Display *dpy = scene.display;
    Drawable drw = scene.drawable;
    GC gc = scene.gc;

    char *firstLabel = "Temperature: ";
    char *secondLabel = "Wind speed: ";
    char *thirdLabel = "Pressure: ";

    int x = 790, y = 170 + 20, height = 26;

    /* Drawing labels */
    XDrawString(dpy, drw, gc, x - 80, y, firstLabel, strlen(firstLabel));
    XDrawString(dpy, drw, gc, x - 73, y + height * 2, secondLabel, strlen(secondLabel));
    XDrawString(dpy, drw, gc, x - 60, y + height * 4, thirdLabel, strlen(thirdLabel));
}

void X_CreateTextAreas()
{
    setUpTextAreaResources();

    XSetWindowAttributes attributes;

    attributes.border_pixel = BlackPixel(scene.display,scene.screen);
    attributes.background_pixel = WhitePixel(scene.display, scene.screen);
    attributes.event_mask = ButtonPressMask;

    int x = 800, y = 170, height = 26;

    scene.textArea1.current = 0;
    scene.textArea2.current = 0;
    scene.textArea3.current = 0;

    /* Creating all text areas */
    scene.textArea1.window = XCreateWindow(
            scene.display, scene.window,
            x, y, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textArea1.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textArea1.window);

    scene.textArea2.window = XCreateWindow(
            scene.display, scene.window,
            x, y + height * 2, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textArea2.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textArea2.window);

    scene.textArea3.window = XCreateWindow(
            scene.display, scene.window,
            x, y + height * 4, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textArea3.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textArea3.window);
}

void E_UpdateCurrentStation()
{
    unsigned int id = currentStationId;
    if (strlen(scene.textArea1.content) > 0)
    {
        sprintf(allStations -> arr[id].data1, "Temperature: %s%cC", scene.textArea1.content, ASCII_DEGREE);
    }
    if (strlen(scene.textArea2.content) > 0)
    {
        sprintf(allStations -> arr[id].data2, "Wind speed : %s km\\h", scene.textArea2.content);
    }
    if (strlen(scene.textArea3.content) > 0)
    {
        sprintf(allStations -> arr[id].data3, "Pressure   : %s hPa", scene.textArea3.content);
    }
}

void E_DrawMap()
{
    /*
    int arr[4] =
        {
            50, 50,
            100, 20
        };

    for (int i = 0; i < sizeof(arr); i += 2)
    {
        if (i == sizeof(arr) - 1)
        {
            break;
        }
        else if (i == sizeof(arr) - 2)
        {
            //XDrawLine(scene.display, scene.window, scene.gc, arr[i], arr[i + 1], arr[0], arr[1]);
        }
        else
        {
            XDrawLine(scene.display, scene.window, scene.gc, arr[i], arr[i + 1], arr[i + 2], arr[i + 3]);
            printf("Drew line!\n");
            //XDrawLine(scene.display, scene.window, scene.gc,  50,  50, 100,  20);
        }
    }
    */
    XDrawLine(scene.display, scene.window, scene.gc,  50,  50, 100,  20);
    XDrawLine(scene.display, scene.window, scene.gc, 100,  20, 250,  25);
    XDrawLine(scene.display, scene.window, scene.gc, 250,  25, 380,  40);
    XDrawLine(scene.display, scene.window, scene.gc, 380,  40, 540,  30);
    XDrawLine(scene.display, scene.window, scene.gc, 540,  30, 610, 120);
    XDrawLine(scene.display, scene.window, scene.gc, 610, 120, 625, 200);
    XDrawLine(scene.display, scene.window, scene.gc, 625, 200, 630, 380);
    XDrawLine(scene.display, scene.window, scene.gc, 630, 380, 570, 600);
    XDrawLine(scene.display, scene.window, scene.gc, 570, 600, 520, 700);
    XDrawLine(scene.display, scene.window, scene.gc, 520, 700, 470, 750);
    XDrawLine(scene.display, scene.window, scene.gc, 470, 750, 370, 760);
    XDrawLine(scene.display, scene.window, scene.gc, 370, 760, 200, 720);
    XDrawLine(scene.display, scene.window, scene.gc, 200, 720, 150, 690);
    XDrawLine(scene.display, scene.window, scene.gc, 150, 690, 100, 700);
    XDrawLine(scene.display, scene.window, scene.gc, 100, 700,  50, 650);
    XDrawLine(scene.display, scene.window, scene.gc,  50, 650,  50, 590);
    XDrawLine(scene.display, scene.window, scene.gc,  50, 590,  20, 400);
    XDrawLine(scene.display, scene.window, scene.gc,  20, 400,  40, 250);
    XDrawLine(scene.display, scene.window, scene.gc,  40, 250,  50,  50);

    XSetLineAttributes(scene.display, scene.gc, 2, LineDoubleDash, CapNotLast, JoinMiter);
}

void E_DisplayStationsData()
{
    Display *dpy = scene.display;
    Window win = scene.window;
    Drawable drw = scene.drawable;
    GC gc = scene.gc;

    int x, y, index, vertSpace = 17;
    int coordinates[COORDINATES_COUNT] =
    {
        230, 70,
        300, 650,
        70, 280,
        460, 350
    };

    unsigned int textWidth = 160;
    unsigned int textHeight = vertSpace * 4;

    // draws data of all allStations
    for (int i = 0; i < COORDINATES_COUNT; i += 2)
    {
        x = coordinates[i];
        y = coordinates[i + 1];
        index = i / 2;

        printf("[DEV]: Current station id: %d\n",  currentStationId);

        // draw circle around current station and draw information label
        if (index == currentStationId)
        {
            XDrawArc(dpy, drw, gc, x - 26, y - 15, 21, 21, 0, 360 * 64);
            char currentStationLabel[26];

            switch (index)
            {
                case 0:
                    sprintf(currentStationLabel, "Current station: Northern");
                    break;
                case 1:
                    sprintf(currentStationLabel, "Current station: Southern");
                    break;
                case 2:
                    sprintf(currentStationLabel, "Current station: Western");
                    break;
                case 3:
                    sprintf(currentStationLabel, "Current station: Eastern");
                    break;
            }
            XDrawString(dpy, drw, gc, 825, 55, currentStationLabel, strlen(currentStationLabel));
        }

        XClearArea(dpy, win, x, y, textWidth, textHeight, 0);
        XFillRectangle(dpy, drw, gc, x - 20, y - 10, 10, 10);
        XDrawString(dpy, drw, gc, x, y, allStations -> arr[index].name,
                    strlen(allStations -> arr[index].name));
        XDrawString(dpy, drw, gc, x, y + vertSpace, allStations -> arr[index].data1,
                    strlen(allStations -> arr[index].data1));
        XDrawString(dpy, drw, gc, x, y + vertSpace * 2, allStations -> arr[index].data2,
                    strlen(allStations -> arr[index].data2));
        XDrawString(dpy, drw, gc, x, y + vertSpace * 3, allStations -> arr[index].data3,
                    strlen(allStations -> arr[index].data3));
    }
}

void E_SetButtonLabel()
{
    Button button = scene.button;
    button.textItem[0].chars = "Update";
    button.textItem[0].nchars = 6;
    button.textItem[0].delta = 0;
    button.textItem[0].font = scene.font -> fid;
    XDrawText(scene.display, scene.button.window, scene.gc,
              (button.width - XTextWidth(scene.font, button.textItem[0].chars, button.textItem[0].nchars)) / 2,
              (button.height - (scene.font -> ascent + scene.font -> descent)) / 2 + scene.font -> ascent,
              button.textItem, 1
    );
}

void E_ClearAllTextAreas()
{
    Display *dpy = scene.display;
    GC gc = scene.gc;

    XClearWindow(dpy, scene.textArea1.window);
    XDrawString(dpy, scene.textArea1.window, gc, 0, 17, &scene.textArea1.content[0], scene.textArea1.end);

    XClearWindow(dpy, scene.textArea2.window);
    XDrawString(dpy, scene.textArea2.window, gc, 0, 17, &scene.textArea2.content[0], scene.textArea2.end);

    XClearWindow(dpy, scene.textArea3.window);
    XDrawString(dpy, scene.textArea3.window, gc, 0, 17, &scene.textArea3.content[0], scene.textArea3.end);
}

void printErrorAndExit(char *message)
{
    printf("[ERROR]: %s\n", message);
    exit(1);
}

int getRandomIntInRange(int lower, int upper)
{
    return rand() % (upper + 1 - lower) + lower;
}

void initializeStations()
{
    Station stationOne;
    sprintf(stationOne.name, "Northern station");

    Station stationTwo;
    sprintf(stationTwo.name, "Southern station");

    Station stationThree;
    sprintf(stationThree.name, "Western station");

    Station stationFour;
    sprintf(stationFour.name, "Eastern station");

    allStations -> arr[0] = stationOne;
    allStations -> arr[1] = stationTwo;
    allStations -> arr[2] = stationThree;
    allStations -> arr[3] = stationFour;

    for (int i = 0; i < 4; i++)
    {
        sprintf(allStations -> arr[i].data1, "Temperature: %d %cC",
                getRandomIntInRange(18, 25), ASCII_DEGREE);
        sprintf(allStations -> arr[i].data2, "Wind speed : %d km\\h",
                getRandomIntInRange(0, 70));
        sprintf(allStations -> arr[i].data3, "Pressure   : %d hPa",
                getRandomIntInRange(1000, 1100));
    }
}

void X_InitSharedMemory()
{
    if((allStationsId = shmget(KEY, sizeof(AllStations), 0666 | IPC_CREAT | IPC_EXCL)) != -1)
    {
        allStations = (AllStations*) shmat(allStationsId, 0, 0);

        initializeStations();

        printf("You are the first client!\n");
    }
    else
    {
        allStationsId = shmget(KEY, sizeof(AllStations), 0666 | IPC_CREAT);
        allStations = (AllStations*) shmat(allStationsId, 0, 0);
        printf("You are the next client!\n");
    }
}

void E_DrawCircleAroundCurrentStation()
{

}

void E_ClearSharedMemoryAndExit()
{
    shmctl(allStationsId, IPC_RMID, 0);
    printf("Memory cleared, exiting...\n");
    exit(0);
}

// Note: drawing MUST be done in the X_EventLoop() function
void X_EventLoop()
{
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

    Display *dpy = scene.display;
    GC gc = scene.gc;
    XEvent event = scene.event;

    int count;
    char bytes[3];
    while (true)
    {
        E_DisplayStationsData();

        XNextEvent(dpy, &event);
        switch (event.type)
        {
            case Expose:
                E_DrawMap();
                E_DisplayStationsData();
                E_DrawTextAreaLabels();
                E_SetButtonLabel();

                break;

            case ButtonPress:
                if (event.xbutton.button == 1)
                {
                    if (event.xany.window == scene.button.window)
                    {
                        printf("[DEV]: Button Pressed!\n");
                        E_UpdateCurrentStation();
                    }
                    else if (event.xany.window == scene.textArea1.window ||
                             event.xany.window == scene.textArea2.window ||
                             event.xany.window == scene.textArea3.window)
                    {
                        printf("[DEV]: Text area selected!\n");

                        E_ClearAllTextAreas();

                        if (event.xany.window == scene.textArea1.window)
                        {
                            /*
                            position = selectedTextArea.position;
                            current = selectedTextArea.current;
                            strcpy(selectedData, dataOne);
                            memset(&selectedData, 0, sizeof(selectedData));
                            */
                            selectedTextArea = &scene.textArea1;
                        }
                        else if (event.xany.window == scene.textArea2.window)
                        {
                            selectedTextArea = &scene.textArea2;
                        }
                        else if (event.xany.window == scene.textArea3.window)
                        {
                            selectedTextArea = &scene.textArea3;
                        }

                        // Modify text area cursor values
                        selectedTextArea -> position = event.xbutton.x / charinc;
                        selectedTextArea -> current = selectedTextArea -> position;
                        if (selectedTextArea -> position > selectedTextArea -> end)
                        {
                            selectedTextArea -> position = selectedTextArea -> end;
                            selectedTextArea -> current = selectedTextArea -> end;
                        }

                        // Draw cursor in mouse click position
                        XCopyArea(dpy, cursor, selectedTextArea -> window, gc, 0, 0, 6, 24,
                                selectedTextArea -> position * charinc, 2);
                    }
                    else
                    {
                        E_ClearAllTextAreas();
                        selectedTextArea = NULL;
                        printf("[DEV]: Background selected!\n");
                    }
                }
                break;

            case KeyPress:
                if ((long) XLookupKeysym (&event.xkey, 0) == 65293 ||
                    (long) XLookupKeysym (&event.xkey, 0) == 65307) // Enter or ESC
                {
                    XCloseDisplay(dpy);
                    E_ClearSharedMemoryAndExit();
                }
                if (selectedTextArea == &scene.textArea1 ||
                    selectedTextArea == &scene.textArea2 ||
                    selectedTextArea == &scene.textArea3)
                {
                    //int current = selectedTextArea -> current;
                    count = XLookupString(&event.xkey, bytes, 3, &character, &xComposeStatus);
                    switch (count)
                    {
                        case 0: // Control character
                            break;

                        case 1: // Printable Character
                            switch (bytes[0])
                            {
                                case 8: // Backspace
                                    if (selectedTextArea -> current - 1 >= 0)
                                        selectedTextArea -> current--;

                                    XClearWindow(dpy, selectedTextArea -> window);
                                    XCopyArea(dpy, cursor, selectedTextArea -> window, gc, 0, 0, 6, 24,
                                              selectedTextArea -> current * charinc, 2);

                                    for (int i = selectedTextArea -> current; i < selectedTextArea -> end; i++)
                                        selectedTextArea -> content[i] = selectedTextArea -> content[i + 1];

                                    if (selectedTextArea -> end - 1 >= 0)
                                        selectedTextArea -> end--;

                                    XDrawString(dpy, selectedTextArea -> window, gc, 0, 17,
                                                &selectedTextArea -> content[0], selectedTextArea -> end);
                                    if (selectedTextArea -> current < 1)
                                        XBell(dpy, 30);
                                    break;

                                case 13: // Enter
                                    XBell(dpy, 30);
                                    break;

                                default: // Any other printable character
                                    // Digits or minus character
                                    if ((bytes[0] >= 48 && bytes[0] <= 57) || bytes[0] == 45)
                                    {
                                        // Text area other than 1 and not a minus character
                                        if ((selectedTextArea -> window != scene.textArea1.window) && bytes[0] == 45)
                                            break;

                                        // Input limit
                                        if (selectedTextArea -> end >= sizeof(selectedTextArea -> content) - 1)
                                        {
                                            printf("[DEV]: Character limit of %lu reached!\n",
                                                    sizeof(selectedTextArea -> content));
                                            break;
                                        }
                                        selectedTextArea -> end++;
                                        for (int i = selectedTextArea -> end; i > selectedTextArea -> current; i--)
                                            selectedTextArea->content[i] = selectedTextArea->content[i - 1];

                                        selectedTextArea -> content[selectedTextArea -> current] = bytes[0];
                                        selectedTextArea -> current++;

                                        XClearWindow(dpy, selectedTextArea -> window);
                                        XCopyArea(dpy, cursor, selectedTextArea -> window, gc, 0, 0, 6, 24,
                                                  selectedTextArea -> current * charinc, 2);
                                        XDrawString(dpy, selectedTextArea -> window, gc, 0, 17,
                                                    &selectedTextArea -> content[0], selectedTextArea -> end);
                                    }
                                    break;
                            }
                            break;
                    }
                }
        }
#pragma clang diagnostic pop
    }
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    if (argc <= 1)
    {
        printErrorAndExit("Please pass a station ID as an argument:\n"
                          "         N - Northern\n"
                          "         S - Southern\n"
                          "         W - Western\n"
                          "         E - Eastern");
    }
    else if (argc > 2)
    {
        printErrorAndExit("Too many arguments");
    }

    if      (strcmp(argv[1], "N") == 0)
        currentStationId = 0;
    else if (strcmp(argv[1], "n") == 0)
        currentStationId = 0;
    else if (strcmp(argv[1], "S") == 0)
        currentStationId = 1;
    else if (strcmp(argv[1], "s") == 0)
        currentStationId = 1;
    else if (strcmp(argv[1], "W") == 0)
        currentStationId = 2;
    else if (strcmp(argv[1], "w") == 0)
        currentStationId = 2;
    else if (strcmp(argv[1], "E") == 0)
        currentStationId = 3;
    else if (strcmp(argv[1], "e") == 0)
        currentStationId = 3;
    else
        printErrorAndExit("Wrong station ID");

    printf("[DEV]: %d\n",  currentStationId);

    X_CreateScene();
    X_SetUpFont();
    X_CreateButton();
    X_CreateTextAreas();

    X_InitSharedMemory();

    X_EventLoop();
}
