// client/include/client_globals.h
#ifndef CLIENT_GLOBALS_H
#define CLIENT_GLOBALS_H

#include <signal.h>   // For sig_atomic_t
#include <stdbool.h>  // For bool type

// Global flag to indicate if SIGINT (Ctrl+C) was received for FULL program exit.
extern volatile sig_atomic_t sigint_received;

// Global flag to indicate if SIGINT (Ctrl+C) was received DURING GAME to exit ONLY THE GAME.
extern volatile sig_atomic_t sigint_game_exit_requested;

// Global flag to indicate if the game is currently running.
// This will be set by client_main.c before calling run_rain_typing_game
// and cleared after it returns.
extern bool is_game_running;

// UI Position Constants
#define Y_TITLE 2
#define Y_OPTIONS_START 4
#define Y_INPUT_FIELD 11
#define Y_STATUS_MSG 13
#define Y_MSG_OFFSET1 1
#define Y_MSG_OFFSET2 2
#define X_DEFAULT_POS 10

// Return codes for auth_ui_main
#define AUTH_UI_EXIT_NORMAL 0
#define AUTH_UI_LOGIN_SUCCESS 1
#define AUTH_UI_EXIT_SIGNAL -2

#endif  // CLIENT_GLOBALS_H