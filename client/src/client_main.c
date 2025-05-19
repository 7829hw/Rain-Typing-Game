#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdbool.h>
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
  timeout(-1);  // Blocking mode by default to avoid flicker

  if (has_colors()) {
    start_color();
  }
}

void end_ncurses_settings() { endwin(); }

void perform_cleanup_and_exit(int exit_code, const char* exit_msg) {
  is_game_running = false;
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
    if (sigint_received) {
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C (application-wide)...");
    }
    sigint_game_exit_requested = 0;

    if (connect_to_server(SERVER_IP, SERVER_PORT) != 0) {
      clear();
      const char* conn_fail_msg = "Failed to connect to server. Press any key to exit.";
      mvprintw(LINES / 2, (COLS - strlen(conn_fail_msg)) / 2, "%s", conn_fail_msg);
      refresh();
      timeout(-1);
      getch();
      timeout(-1);
      perform_cleanup_and_exit(EXIT_FAILURE, "Server connection failed.");
    }

    if (sigint_received) {
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C (application-wide)...");
    }

    int auth_result = auth_ui_main(user_id);

    if (sigint_received || auth_result == AUTH_UI_EXIT_SIGNAL) {
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C during auth (application-wide)...");
    }

    if (auth_result == AUTH_UI_EXIT_NORMAL) {
      perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game by user choice from auth menu.");
    }

    if (strlen(user_id) > 0) {
      bool stay_in_menu = true;
      while (stay_in_menu) {
        if (sigint_received) {
          stay_in_menu = false;
          break;
        }
        sigint_game_exit_requested = 0;

        // Draw menu using off-screen buffer to reduce flicker
        erase();
        mvprintw(Y_TITLE, X_DEFAULT_POS, "Logged in as: %s", user_id);
        mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "1. Start Game");
        mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "2. View Leaderboard");
        mvprintw(Y_OPTIONS_START + 2, X_DEFAULT_POS, "3. Logout");
        mvprintw(Y_OPTIONS_START + 3, X_DEFAULT_POS, "4. Exit Game");
        mvprintw(Y_OPTIONS_START + 5, X_DEFAULT_POS, "Select an option: ");
        wnoutrefresh(stdscr);
        doupdate();

        timeout(-1);  // Blocking input to avoid continuous redraw
        int choice = getch();

        switch (choice) {
          case '1': {
            is_game_running = true;
            clear();
            refresh();
            int final_score = run_rain_typing_game(user_id);
            is_game_running = false;

            if (sigint_received) {
              stay_in_menu = false;
              break;
            }

            clear();
            refresh();
            if (final_score < 0) {
              mvprintw(Y_STATUS_MSG - 2, X_DEFAULT_POS, "Game could not start or was aborted. (Error: %d)", final_score);
            } else {
              mvprintw(Y_STATUS_MSG - 4, X_DEFAULT_POS, "Game Over! Your final score: %d", final_score);
              ScoreSubmitResponse score_res;
              int ret = send_score_submit_request(final_score, &score_res);

              if (sigint_received) {
                stay_in_menu = false;
                break;
              }

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
            }
            break;
          }
          case '2':
            show_leaderboard_ui();
            if (sigint_received) {
              stay_in_menu = false;
            }
            break;
          case '3': {
            LogoutResponse logout_res;
            int ret = send_logout_request(&logout_res);
            if (sigint_received) {
              stay_in_menu = false;
              break;
            }
            clear();
            if (ret == 0 && logout_res.success) {
              mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Logout successful. %s", logout_res.message);
              user_id[0] = '\0';
              stay_in_menu = false;
            } else {
              mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Logout failed. Server: %s (ret: %d)", (ret != 0 ? "Network/Comm error" : logout_res.message),
                       ret);
            }
            wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET2, X_DEFAULT_POS, "Press any key to continue...");
            if (sigint_received) {
              stay_in_menu = false;
            }
            break;
          }
          case '4': {
            clear();
            const char* exit_confirm_msg = "Are you sure you want to exit? (y/n)";
            mvprintw(LINES / 2 - 1, (COLS - strlen(exit_confirm_msg)) / 2, "%s", exit_confirm_msg);
            refresh();
            timeout(-1);
            int confirm = getch();
            timeout(-1);

            if (confirm == 'y' || confirm == 'Y') {
              perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game by user's choice.");
            }
            if (sigint_received) {
              stay_in_menu = false;
            }
            break;
          }
          default:
            mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Invalid option. Press any key to try again.");
            wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "");
            if (sigint_received) {
              stay_in_menu = false;
            }
            break;
        }
      }
      if (sigint_received) {
        perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting due to Ctrl+C from main menu (application-wide)...");
      }
    }
    disconnect_from_server();
  }

  perform_cleanup_and_exit(EXIT_SUCCESS, "Exiting game (application-wide).");
  return EXIT_SUCCESS;
}
