#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define true 1
#define false 0

#define cursoricon_width 6
#define cursoricon_heigth 24

#define ASCII_DEGREE 176

// cursor shape
static unsigned char cursoricon_bits[] =
{
        0x21, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
        0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x1e, 0x21
};

#define COORDINATES_COUNT 8

enum
{
    WIN_X = 100,
    WIN_Y = 500,
    WIN_WIDTH = 1000,
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
}
TextArea;

typedef struct
{
    //useless
    Window window;
    Window rootWindow;

    int x, y, width, height;
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
    TextArea textAreaOne;
    TextArea textAreaTwo;
    TextArea textAreaThree;

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

/* global scene */
Scene scene;

// colors
int red;

int count;
int charinc, position, end, current;

char selectedData[4], bytes[3];
char dataTemperature[4];
char dataWindSpeed[4];
char dataPressure[4];

KeySym character;
XComposeStatus xComposeStatus;
Pixmap cursor;

int selectedTextArea;

// Stations selectedData
AllStations stations;

/* FUNCTIONS */

// NAMING CONVENTIONS:
//   camelCase - function to use in X or E function
//   X_PascalCase - function to use in main() function
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

    scene.windowAttributes.event_mask = ExposureMask | KeyPressMask;
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
            WIN_WIDTH - 200, 100, 50, 25, WIN_BORDER,
            BlackPixel(scene.display, scene.screen), scene.color_gray.pixel
    );

    XSelectInput(
            scene.display, scene.button.window,
            ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask
    );

    // display the buttonWindow
    XMapWindow(scene.display, scene.button.window);

    // center text?
    XGetGeometry(
            scene.display, scene.button.window, &scene.button.rootWindow,
            &scene.button.x, &scene.button.y, &scene.button.width, &scene.button.height,
            &scene.button.border_width, &scene.button.depth
    );
}

void setUpTextAreaResources()
{
    //co to robi? width of char?
    charinc = scene.font -> per_char -> width;

    XAllocNamedColor(scene.display, scene.colormap, "red", &scene.exact, &scene.closest);
    red = scene.closest.pixel;

    cursor = XCreatePixmapFromBitmapData(
            scene.display, scene.window,
            cursoricon_bits, cursoricon_width, cursoricon_heigth,
            red, WhitePixel(scene.display, scene.screen),
            scene.depth
            );
}

//not working
void createTextArea(Window window, XSetWindowAttributes attributes, int y, int height)
{
    window = XCreateWindow(
            scene.display, scene.window,
            700, y, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, window);
}

void X_CreateTextAreas()
{
    setUpTextAreaResources();

    XSetWindowAttributes attributes;

    attributes.border_pixel = BlackPixel(scene.display,scene.screen);
    attributes.background_pixel = WhitePixel(scene.display, scene.screen);
    attributes.event_mask = ButtonPressMask | ButtonReleaseMask;// | ExposureMask | KeyPressMask;

    int y = 170, height = 26;

    scene.textAreaOne.window = XCreateWindow(
            scene.display, scene.window,
            700, y, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textAreaOne.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textAreaOne.window);

    scene.textAreaTwo.window = XCreateWindow(
            scene.display, scene.window,
            700, y + height * 2, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textAreaTwo.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textAreaTwo.window);

    scene.textAreaThree.window = XCreateWindow(
            scene.display, scene.window,
            700, y+ height * 4, 220, height, 2,
            scene.depth, InputOutput, scene.visual,
            scene.mask, &attributes
    );

    XSelectInput(
            scene.display, scene.textAreaThree.window,
            attributes.event_mask
    );

    // display the window
    XMapWindow(scene.display, scene.textAreaThree.window);

    //createTextArea(scene.textAreaOne.window, attributes, y, height);
    //createTextArea(scene.textAreaTwo.window, attributes, y + height * 2, height);
    //createTextArea(scene.textAreaThree.window, attributes, y + height * 4, height);

}

void X_InitializeStations()
{
    Station stationOne;
    sprintf(stationOne.name, "Northern station");

    Station stationTwo;
    sprintf(stationTwo.name, "Southern station");

    Station stationThree;
    sprintf(stationThree.name, "Western station");

    Station stationFour;
    sprintf(stationFour.name, "Eastern station");

    stations.arr[0] = stationOne;
    stations.arr[1] = stationTwo;
    stations.arr[2] = stationThree;
    stations.arr[3] = stationFour;
}

void E_UpdateAllStations()
{
    /*
    char str[stringLength] = *stations;
    char *str = malloc(stringSize);
    sprintf(str, *stationsPtr);

    if (userCount == 0)
      stations = getDefaultValues();
    else
    */

    for (int i = 0; i < 4; i++)
    {
        sprintf(stations.arr[i].data1, "Temperature: %s %cC", selectedData, ASCII_DEGREE);
        sprintf(stations.arr[i].data2, "Wind speed : %s km\\h", selectedData);
        sprintf(stations.arr[i].data3, "Pressure   : %s hPa", selectedData);
    }

}

void E_UpdateCurrentStation(int currentStationId)
{
    unsigned int id = currentStationId;
    if (strlen(selectedData) > 0)
    {
        sprintf(stations.arr[id].data1, "Temperature: %s%cC", selectedData, ASCII_DEGREE);
    }
    //sprintf(stations.arr[id].data2, "Wind speed : %s km\\h", selectedData);
    //sprintf(stations.arr[id].data3, "Pressure   : %s hPa", selectedData);

}

void E_DrawMap()
{
    //XSetForeground(scene.display, scene.gc)
    XDrawLine(scene.display, scene.window, scene.gc, 50 , 50 , 100, 20 );
    XDrawLine(scene.display, scene.window, scene.gc, 100, 20 , 250, 25 );
    XDrawLine(scene.display, scene.window, scene.gc, 250, 25 , 380, 40 );
    XDrawLine(scene.display, scene.window, scene.gc, 380, 40 , 540, 30 );
    XDrawLine(scene.display, scene.window, scene.gc, 540, 30 , 610, 120);
    XDrawLine(scene.display, scene.window, scene.gc, 610, 120, 600, 200);
    XDrawLine(scene.display, scene.window, scene.gc, 600, 200, 600, 380);
    XDrawLine(scene.display, scene.window, scene.gc, 600, 380, 570, 600);
    XDrawLine(scene.display, scene.window, scene.gc, 570, 600, 520, 700);
    XDrawLine(scene.display, scene.window, scene.gc, 520, 700, 470, 750);
    XDrawLine(scene.display, scene.window, scene.gc, 470, 750, 370, 760);
    XDrawLine(scene.display, scene.window, scene.gc, 370, 760, 200, 720);
    XDrawLine(scene.display, scene.window, scene.gc, 200, 720, 150, 690);
    XDrawLine(scene.display, scene.window, scene.gc, 150, 690, 100, 700);
    XDrawLine(scene.display, scene.window, scene.gc, 100, 700, 50 , 650);
    XDrawLine(scene.display, scene.window, scene.gc, 50 , 650, 50, 590);
    XDrawLine(scene.display, scene.window, scene.gc, 50, 590, 20, 400);
    XDrawLine(scene.display, scene.window, scene.gc, 20, 400, 40, 250);
    XDrawLine(scene.display, scene.window, scene.gc, 40, 250, 50 , 50);

    XSetLineAttributes(scene.display, scene.gc, 2, LineDoubleDash, CapNotLast, JoinMiter);
}

void E_DrawStations()
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

    // draws selectedData of all stations
    for (int i = 0; i < COORDINATES_COUNT; i += 2)
    {
        x = coordinates[i];
        y = coordinates[i + 1];
        index = i / 2;

        //XFillRectangle(dpy, drw, gc, x, y - 10, textWidth, textHeight);
        XClearArea(dpy, win, x, y, textWidth, textHeight, 0);
        XFillRectangle(dpy, drw, gc, x - 20, y - 10, 10, 10);
        XDrawString(dpy, drw, gc, x, y, stations.arr[index].name, strlen(stations.arr[index].name));
        XDrawString(dpy, drw, gc, x, y + vertSpace, stations.arr[index].data1, strlen(stations.arr[index].data1));
        XDrawString(dpy, drw, gc, x, y + vertSpace * 2, stations.arr[index].data2, strlen(stations.arr[index].data2));
        XDrawString(dpy, drw, gc, x, y + vertSpace * 3, stations.arr[index].data3, strlen(stations.arr[index].data3));
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

// Note: drawing MUST be done in the X_EventLoop() function
void X_EventLoop()
{
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

    Display *dpy = scene.display;
    Drawable drw = scene.drawable;
    GC gc = scene.gc;
    XEvent event = scene.event;


    Window selectedWindow;
    Window prevSelectedWindow;
    current = end = 0;
    while (true)
    {
        XNextEvent(dpy, &event);
        switch (event.type)
        {
            case Expose:
                //XSetForeground(scene.display, scene.gc, BlackPixel(scene.display, scene.screen));

                E_DrawMap();
                E_UpdateAllStations();
                E_DrawStations();

                E_SetButtonLabel();
                break;

            case ButtonPress:
                if (event.xbutton.button == 1)
                {
                    if (event.xany.window == scene.button.window)
                    {
                        printf("Button Pressed!\n");
                        E_UpdateCurrentStation(NORTH);
                        E_DrawStations();
                    }
                    else if (event.xany.window == scene.textAreaOne.window ||
                             event.xany.window == scene.textAreaTwo.window ||
                             event.xany.window == scene.textAreaThree.window)
                    {
                        printf("Text area pressed!\n");

                        // Clear all text areas
                        XClearWindow(dpy, scene.textAreaOne.window);
                        XDrawString(dpy, scene.textAreaOne.window, gc, 0, 17, &selectedData[0], end);
                        XClearWindow(dpy, scene.textAreaTwo.window);
                        XDrawString(dpy, scene.textAreaTwo.window, gc, 0, 17, &selectedData[0], end);
                        XClearWindow(dpy, scene.textAreaThree.window);
                        XDrawString(dpy, scene.textAreaThree.window, gc, 0, 17, &selectedData[0], end);

                        if (event.xany.window == scene.textAreaOne.window)
                            selectedWindow = scene.textAreaOne.window;
                        else if (event.xany.window == scene.textAreaTwo.window)
                            selectedWindow = scene.textAreaTwo.window;
                        else if (event.xany.window == scene.textAreaThree.window)
                            selectedWindow = scene.textAreaThree.window;

                        // Draw cursor
                        position = event.xbutton.x / charinc;
                        current = position;
                        if (position > end)
                        {
                            position = end;
                            current = end;
                        }
                        XClearWindow(dpy, selectedWindow);
                        XCopyArea(dpy, cursor, selectedWindow, gc, 0, 0, 6, 24, position * charinc, 2);
                        XDrawString(dpy, selectedWindow, gc, 0, 17, &selectedData[0], end);
                    }
                }
                break;

            case KeyPress:
                /*
                if (selectedWindow)
                {
                    break;
                }
*/
                count = XLookupString(&event.xkey, bytes, 3, &character, &xComposeStatus);
                switch (count)
                {
                    case 0: // Control character
                        break;

                    case 1: // Printable Character
                        switch (bytes[0])
                        {
                            case 8: // Backspace
                                if (current - 1 >= 0)
                                    current--;

                                XClearWindow(dpy, selectedWindow);
                                XCopyArea(dpy, cursor, selectedWindow, gc, 0, 0, 6, 24, current * charinc, 2);

                                for (int i = current; i < end; i++)
                                    selectedData[i] = selectedData[i + 1];

                                if (end - 1 >= 0)
                                    end--;

                                XDrawString(dpy, selectedWindow, gc, 0, 17, &selectedData[0], end);
                                if (current < 1)
                                    XBell(dpy, 30);

                                break;

                            case 13: // Enter
                                XBell(dpy, 30);
                                break;

                            default: // Any other printable character
                                // Digits or minus character
                                if (bytes[0] >= 48 && bytes[0] <= 57 || bytes[0] == 45)
                                {
                                    // Text area other than 1 and not a minus character
                                    if ((selectedWindow != scene.textAreaOne.window) && bytes[0] == 45)
                                    {
                                        break;
                                    }

                                    end++;
                                    for (int i = end; i > current; i--)
                                        selectedData[i] = selectedData[i - 1];

                                    selectedData[current] = bytes[0];
                                    current++;

                                    XClearWindow(dpy, selectedWindow);
                                    XCopyArea(dpy, cursor, selectedWindow, gc, 0, 0, 6, 24, current * charinc,
                                              2);
                                    XDrawString(dpy, selectedWindow, gc, 0, 17, &selectedData[0], end);
                                }
                                break;
                        }
                        printf("selectedData: %s\n", &selectedData[0]);
                        break;
                }
        }
#pragma clang diagnostic pop
    }
}

int main(int argc, char *argv[])
{
    X_CreateScene();
    X_SetUpFont();
    X_CreateButton();
    X_CreateTextAreas();
    X_InitializeStations();
    X_EventLoop();
}
