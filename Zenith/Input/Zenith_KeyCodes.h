#pragma once

using Zenith_KeyCode = int32_t;

//#TO stolen from glfw
#define ZENITH_KEY_SPACE              32
#define ZENITH_KEY_APOSTROPHE         39  /* ' */
#define ZENITH_KEY_COMMA              44  /* , */
#define ZENITH_KEY_MINUS              45  /* - */
#define ZENITH_KEY_PERIOD             46  /* . */
#define ZENITH_KEY_SLASH              47  /* / */
#define ZENITH_KEY_0                  48
#define ZENITH_KEY_1                  49
#define ZENITH_KEY_2                  50
#define ZENITH_KEY_3                  51
#define ZENITH_KEY_4                  52
#define ZENITH_KEY_5                  53
#define ZENITH_KEY_6                  54
#define ZENITH_KEY_7                  55
#define ZENITH_KEY_8                  56
#define ZENITH_KEY_9                  57
#define ZENITH_KEY_SEMICOLON          59  /* ; */
#define ZENITH_KEY_EQUAL              61  /* = */
#define ZENITH_KEY_A                  65
#define ZENITH_KEY_B                  66
#define ZENITH_KEY_C                  67
#define ZENITH_KEY_D                  68
#define ZENITH_KEY_E                  69
#define ZENITH_KEY_F                  70
#define ZENITH_KEY_G                  71
#define ZENITH_KEY_H                  72
#define ZENITH_KEY_I                  73
#define ZENITH_KEY_J                  74
#define ZENITH_KEY_K                  75
#define ZENITH_KEY_L                  76
#define ZENITH_KEY_M                  77
#define ZENITH_KEY_N                  78
#define ZENITH_KEY_O                  79
#define ZENITH_KEY_P                  80
#define ZENITH_KEY_Q                  81
#define ZENITH_KEY_R                  82
#define ZENITH_KEY_S                  83
#define ZENITH_KEY_T                  84
#define ZENITH_KEY_U                  85
#define ZENITH_KEY_V                  86
#define ZENITH_KEY_W                  87
#define ZENITH_KEY_X                  88
#define ZENITH_KEY_Y                  89
#define ZENITH_KEY_Z                  90
#define ZENITH_KEY_LEFT_BRACKET       91  /* [ */
#define ZENITH_KEY_BACKSLASH          92  /* \ */
#define ZENITH_KEY_RIGHT_BRACKET      93  /* ] */
#define ZENITH_KEY_GRAVE_ACCENT       96  /* ` */
#define ZENITH_KEY_WORLD_1            161 /* non-US #1 */
#define ZENITH_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define ZENITH_KEY_ESCAPE             256
#define ZENITH_KEY_ENTER              257
#define ZENITH_KEY_TAB                258
#define ZENITH_KEY_BACKSPACE          259
#define ZENITH_KEY_INSERT             260
#define ZENITH_KEY_DELETE             261
#define ZENITH_KEY_RIGHT              262
#define ZENITH_KEY_LEFT               263
#define ZENITH_KEY_DOWN               264
#define ZENITH_KEY_UP                 265
#define ZENITH_KEY_PAGE_UP            266
#define ZENITH_KEY_PAGE_DOWN          267
#define ZENITH_KEY_HOME               268
#define ZENITH_KEY_END                269
#define ZENITH_KEY_CAPS_LOCK          280
#define ZENITH_KEY_SCROLL_LOCK        281
#define ZENITH_KEY_NUM_LOCK           282
#define ZENITH_KEY_PRINT_SCREEN       283
#define ZENITH_KEY_PAUSE              284
#define ZENITH_KEY_F1                 290
#define ZENITH_KEY_F2                 291
#define ZENITH_KEY_F3                 292
#define ZENITH_KEY_F4                 293
#define ZENITH_KEY_F5                 294
#define ZENITH_KEY_F6                 295
#define ZENITH_KEY_F7                 296
#define ZENITH_KEY_F8                 297
#define ZENITH_KEY_F9                 298
#define ZENITH_KEY_F10                299
#define ZENITH_KEY_F11                300
#define ZENITH_KEY_F12                301
#define ZENITH_KEY_F13                302
#define ZENITH_KEY_F14                303
#define ZENITH_KEY_F15                304
#define ZENITH_KEY_F16                305
#define ZENITH_KEY_F17                306
#define ZENITH_KEY_F18                307
#define ZENITH_KEY_F19                308
#define ZENITH_KEY_F20                309
#define ZENITH_KEY_F21                310
#define ZENITH_KEY_F22                311
#define ZENITH_KEY_F23                312
#define ZENITH_KEY_F24                313
#define ZENITH_KEY_F25                314
#define ZENITH_KEY_KP_0               320
#define ZENITH_KEY_KP_1               321
#define ZENITH_KEY_KP_2               322
#define ZENITH_KEY_KP_3               323
#define ZENITH_KEY_KP_4               324
#define ZENITH_KEY_KP_5               325
#define ZENITH_KEY_KP_6               326
#define ZENITH_KEY_KP_7               327
#define ZENITH_KEY_KP_8               328
#define ZENITH_KEY_KP_9               329
#define ZENITH_KEY_KP_DECIMAL         330
#define ZENITH_KEY_KP_DIVIDE          331
#define ZENITH_KEY_KP_MULTIPLY        332
#define ZENITH_KEY_KP_SUBTRACT        333
#define ZENITH_KEY_KP_ADD             334
#define ZENITH_KEY_KP_ENTER           335
#define ZENITH_KEY_KP_EQUAL           336
#define ZENITH_KEY_LEFT_SHIFT         340
#define ZENITH_KEY_LEFT_CONTROL       341
#define ZENITH_KEY_LEFT_ALT           342
#define ZENITH_KEY_LEFT_SUPER         343
#define ZENITH_KEY_RIGHT_SHIFT        344
#define ZENITH_KEY_RIGHT_CONTROL      345
#define ZENITH_KEY_RIGHT_ALT          346
#define ZENITH_KEY_RIGHT_SUPER        347
#define ZENITH_KEY_MENU               348

#define ZENITH_MOUSE_BUTTON_1         0
#define ZENITH_MOUSE_BUTTON_2         1
#define ZENITH_MOUSE_BUTTON_3         2
#define ZENITH_MOUSE_BUTTON_4         3
#define ZENITH_MOUSE_BUTTON_5         4
#define ZENITH_MOUSE_BUTTON_6         5
#define ZENITH_MOUSE_BUTTON_7         6
#define ZENITH_MOUSE_BUTTON_8         7
#define ZENITH_MOUSE_BUTTON_LAST      ZENITH_MOUSE_BUTTON_8
#define ZENITH_MOUSE_BUTTON_LEFT      ZENITH_MOUSE_BUTTON_1
#define ZENITH_MOUSE_BUTTON_RIGHT     ZENITH_MOUSE_BUTTON_2
#define ZENITH_MOUSE_BUTTON_MIDDLE    ZENITH_MOUSE_BUTTON_3

// Gamepad buttons (matching GLFW_GAMEPAD_BUTTON_* values)
#define ZENITH_GAMEPAD_BUTTON_A               0
#define ZENITH_GAMEPAD_BUTTON_B               1
#define ZENITH_GAMEPAD_BUTTON_X               2
#define ZENITH_GAMEPAD_BUTTON_Y               3
#define ZENITH_GAMEPAD_BUTTON_LEFT_BUMPER     4
#define ZENITH_GAMEPAD_BUTTON_RIGHT_BUMPER    5
#define ZENITH_GAMEPAD_BUTTON_BACK            6
#define ZENITH_GAMEPAD_BUTTON_START           7
#define ZENITH_GAMEPAD_BUTTON_GUIDE           8
#define ZENITH_GAMEPAD_BUTTON_LEFT_THUMB      9
#define ZENITH_GAMEPAD_BUTTON_RIGHT_THUMB     10
#define ZENITH_GAMEPAD_BUTTON_DPAD_UP         11
#define ZENITH_GAMEPAD_BUTTON_DPAD_RIGHT      12
#define ZENITH_GAMEPAD_BUTTON_DPAD_DOWN       13
#define ZENITH_GAMEPAD_BUTTON_DPAD_LEFT       14
#define ZENITH_GAMEPAD_BUTTON_LAST            ZENITH_GAMEPAD_BUTTON_DPAD_LEFT

// PlayStation-style aliases
#define ZENITH_GAMEPAD_BUTTON_CROSS       ZENITH_GAMEPAD_BUTTON_A
#define ZENITH_GAMEPAD_BUTTON_CIRCLE      ZENITH_GAMEPAD_BUTTON_B
#define ZENITH_GAMEPAD_BUTTON_SQUARE      ZENITH_GAMEPAD_BUTTON_X
#define ZENITH_GAMEPAD_BUTTON_TRIANGLE    ZENITH_GAMEPAD_BUTTON_Y

// Gamepad axes (matching GLFW_GAMEPAD_AXIS_* values)
#define ZENITH_GAMEPAD_AXIS_LEFT_X        0
#define ZENITH_GAMEPAD_AXIS_LEFT_Y        1
#define ZENITH_GAMEPAD_AXIS_RIGHT_X       2
#define ZENITH_GAMEPAD_AXIS_RIGHT_Y       3
#define ZENITH_GAMEPAD_AXIS_LEFT_TRIGGER  4
#define ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define ZENITH_GAMEPAD_AXIS_LAST          ZENITH_GAMEPAD_AXIS_RIGHT_TRIGGER