// client/src/client_main.c
#include <locale.h>
#include <ncursesw/curses.h>
#include <signal.h>
#include <stdbool.h>  // For bool type
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth_ui.h"
#include "client_globals.h"
#include "client_network.h"
#include "game_logic.h"
#include "leaderboard_ui.h"
#include "protocol.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

volatile sig_atomic_t sigint_received = 0;
volatile sig_atomic_t sigint_game_exit_requested = 0;  // New flag for game-specific exit
bool is_game_running = false;                          // Flag to indicate game state for SIGINT handler

void handle_sigint(int sig) {
  (void)sig;
  if (is_game_running) {
    sigint_game_exit_requested = 1;  // Request to exit only the game
  } else {
    sigint_received = 1;  // Request to exit the entire application
  }
}

void wait_for_key_or_signal(int y, int x, const char* prompt) {
  mvprintw(y, x, "%s", prompt);
  clrtoeol();
  refresh();
  int key;
  do {
    // If game exit was requested by SIGINT, or full exit, break
    if (sigint_game_exit_requested || sigint_received) break;
    key = getch();
  } while (key == ERR);
}

void init_ncurses_settings() {
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0);
  timeout(100);

  if (has_colors()) {
    start_color();
  }
}

void end_ncurses_settings() { endwin(); }

void perform_cleanup_and_exit(int exit_code, const char* exit_msg) {
  is_game_running = false;  // Ensure game is marked as not running
  disconnect_from_server();
  end_ncurses_settings();
  if (exit_msg) {
    printf("%s\n", exit_msg);
  }
  exit(exit_code);
}

int main() {
  signal(SIGINT, handle_sigint);
  init_ncurses_settings();

  char user_id[MAX_ID_LEN] = {0};

  while (1) {
    if (sigint_received) {  // Full program exit
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C (application-wide)...");
    }
    // Reset game-specific SIGINT flag if we are back in the main loop
    sigint_game_exit_requested = 0;

    if (connect_to_server(SERVER_IP, SERVER_PORT) != 0) {
      clear();
      const char* conn_fail_msg = "Failed to connect to server. Press any key to exit.";
      mvprintw(LINES / 2, (COLS - strlen(conn_fail_msg)) / 2, "%s", conn_fail_msg);
      refresh();
      timeout(-1);
      getch();       // Blocking wait
      timeout(100);  // Restore non-blocking
      perform_cleanup_and_exit(EXIT_FAILURE, "Server connection failed.");
    }

    if (sigint_received) {  // Check again after potential block
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C (application-wide)...");
    }

    int auth_result = auth_ui_main(user_id);  // auth_ui_main also needs to check sigint_received

    if (sigint_received || auth_result == AUTH_UI_EXIT_SIGNAL) {  // AUTH_UI_EXIT_SIGNAL means auth UI was interrupted by SIGINT
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C during auth (application-wide)...");
    }

    if (auth_result == AUTH_UI_EXIT_NORMAL) {  // User chose 'Exit' in auth menu
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game by user choice from auth menu.");
    }

    if (strlen(user_id) > 0) {  // Login successful
      bool stay_in_menu = true;
      while (stay_in_menu) {
        if (sigint_received) {  // Full program exit
          stay_in_menu = false;
          break;
        }
        sigint_game_exit_requested = 0;  // Reset before menu display

        clear();
        mvprintw(Y_TITLE, X_DEFAULT_POS, "Logged in as: %s", user_id);
        mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "1. Start Game");
        mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "2. View Leaderboard");
        mvprintw(Y_OPTIONS_START + 2, X_DEFAULT_POS, "3. Logout");
        mvprintw(Y_OPTIONS_START + 3, X_DEFAULT_POS, "4. Exit Game");
        mvprintw(Y_OPTIONS_START + 5, X_DEFAULT_POS, "Select an option: ");
        refresh();

        int choice = getch();
        if (choice == ERR) {  // Timeout, no input
          if (sigint_received) {
            stay_in_menu = false;
            break;
          }  // Check full exit signal
          continue;
        }

        switch (choice) {
          case '1': {                // Start Game
            is_game_running = true;  // Set flag before starting game
            clear();
            refresh();
            int final_score = run_rain_typing_game(user_id);  // This will now primarily check sigint_game_exit_requested
            is_game_running = false;                          // Clear flag after game finishes or is exited

            if (sigint_received) {  // If a full exit was requested DURING the game (rare, but possible if handler logic changes)
              stay_in_menu = false;
              break;
            }
            // If only game exit was requested, sigint_game_exit_requested would have been handled inside run_rain_typing_game
            // and it would return, allowing us to proceed to score submission or menu.
            // We reset sigint_game_exit_requested at the start of the menu loop.

            clear();
            refresh();

            if (final_score < 0) {  // Game error (e.g. screen too small) or aborted not by player score
              mvprintw(Y_STATUS_MSG - 2, X_DEFAULT_POS, "Game could not start or was aborted. (Error: %d)", final_score);
            } else {
              // Even if game was exited by Ctrl+C (game only), final_score would be the score at that point.
              mvprintw(Y_STATUS_MSG - 4, X_DEFAULT_POS, "Game Over! Your final score: %d", final_score);
              ScoreSubmitResponse score_res;
              int ret = send_score_submit_request(final_score, &score_res);  // This might be interrupted by sigint_received

              if (sigint_received) {
                stay_in_menu = false;
                break;
              }  // Check after network op

              if (ret == 0 && score_res.success) {
                mvprintw(Y_STATUS_MSG - 2, X_DEFAULT_POS, "Score submitted successfully! Server: %s", score_res.message);
              } else {
                mvprintw(Y_STATUS_MSG - 2, X_DEFAULT_POS, "Failed to submit score. Server: %s (ret: %d)",
                         (ret != 0 ? "Network/Comm error" : score_res.message), ret);
              }
            }
            wait_for_key_or_signal(Y_STATUS_MSG, X_DEFAULT_POS, "Press any key to return to the menu...");
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            break;
          }
          case '2':                 // View Leaderboard
            show_leaderboard_ui();  // This also needs to handle sigint_received internally via wait_for_key_or_signal
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            break;
          case '3': {  // Logout
            LogoutResponse logout_res;
            int ret = send_logout_request(&logout_res);
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }  // Check after network op
            clear();
            if (ret == 0 && logout_res.success) {
              mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Logout successful. %s", logout_res.message);
              user_id[0] = '\0';
              stay_in_menu = false;  // Exit menu loop to go to auth screen
            } else {
              mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Logout failed. Server: %s (ret: %d)", (ret != 0 ? "Network/Comm error" : logout_res.message),
                       ret);
            }
            wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to continue...");
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            break;
          }
          case '4': {  // Exit Game (application)
            clear();
            const char* exit_confirm_msg = "Are you sure you want to exit? (y/n)";
            mvprintw(LINES / 2 - 1, (COLS - strlen(exit_confirm_msg)) / 2, "%s", exit_confirm_msg);
            refresh();
            timeout(-1);  // Blocking wait for y/n
            int confirm = getch();
            timeout(100);  // Restore default timeout

            if (confirm == 'y' || confirm == 'Y') {
              perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game by user's choice.");
            }
            // If 'n' or other, or if sigint_received during the blocking getch,
            // the outer loop will catch sigint_received.
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            break;
          }
          default:
            mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Invalid option. Press any key to try again.");
            wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "");  // Just waits
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            break;
        }
      }  // end while(stay_in_menu)
      if (sigint_received) {  // If SIGINT (full exit) broke the menu loop
        perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C from main menu (application-wide)...");
      }
    }
    disconnect_from_server();  // Disconnect if logged out or auth failed before next auth attempt
  }

  perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game (application-wide).");  // Should ideally not be reached if SIGINT is well-handled
  return EXIT_SUCCESS;
}