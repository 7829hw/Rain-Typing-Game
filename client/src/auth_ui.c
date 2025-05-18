// client/src/auth_ui.c
#include "auth_ui.h"

#include <ctype.h>
#include <ncursesw/curses.h>
#include <stdlib.h>
#include <string.h>

#include "client_globals.h"
#include "client_network.h"  // 변경: network_client.h -> client_network.h
#include "protocol.h"

// Forward declaration for helper from client_main.c
void wait_for_key_or_signal(int y, int x, const char* prompt);

void draw_auth_menu() {
  clear();
  mvprintw(Y_TITLE, X_DEFAULT_POS, "=== Welcome to Rain Typing Game ===");
  mvprintw(Y_OPTIONS_START, X_DEFAULT_POS, "1. Login");
  mvprintw(Y_OPTIONS_START + 1, X_DEFAULT_POS, "2. Register");
  mvprintw(Y_OPTIONS_START + 2, X_DEFAULT_POS, "3. Exit");
  mvprintw(Y_OPTIONS_START + 4, X_DEFAULT_POS, "Select an option: ");
  refresh();
}

// Custom input function that handles SIGINT and ncurses timeout
static int custom_get_string(const char* label, char* buffer, int max_len, bool is_password) {
  int current_y = Y_INPUT_FIELD;
  int label_x = X_DEFAULT_POS;
  int input_start_x = label_x + strlen(label) + 2;  // "ID: " or "PW: "
  int ch, i = 0;

  buffer[0] = '\0';  // Clear buffer initially

  if (!is_password) {
    echo();  // Turn on echo for normal input
  }  // noecho() is default from main_client settings, and for password

  // Print label and clear rest of the input line
  mvprintw(current_y, label_x, "%s: ", label);
  move(current_y, input_start_x);
  clrtoeol();
  refresh();

  while (i < max_len - 1) {
    if (sigint_received) {
      if (!is_password) noecho();  // Restore noecho if it was turned on
      return -1;                   // Interrupted
    }

    ch = getch();  // Relies on global timeout(100)

    if (ch == ERR) {  // Timeout or no input
      continue;
    }

    if (ch == '\n' || ch == KEY_ENTER) {
      break;  // End of input
    } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
      if (i > 0) {
        i--;
        // Move cursor, print space, move cursor back
        mvaddch(current_y, input_start_x + i, ' ');
        move(current_y, input_start_x + i);
      }
    } else if (isprint(ch)) {  // Process printable characters
      buffer[i++] = ch;
      if (is_password) {
        mvaddch(current_y, input_start_x + i - 1, '*');
      } else {  // Normal echo handled by ncurses echo()
        mvaddch(current_y, input_start_x + i - 1, ch);
      }
    }
    refresh();  // Refresh after each character
  }
  buffer[i] = '\0';  // Null-terminate

  if (!is_password) {
    noecho();  // Restore noecho if it was turned on for plain input
  }

  // Clear from end of input to end of line
  move(current_y, input_start_x + i);
  clrtoeol();
  refresh();

  return 0;  // Success
}

void input_plain(const char* label, char* buffer, int max_len) {
  if (custom_get_string(label, buffer, max_len, false) == -1) {
    // SIGINT was received during input, auth_ui_main will check global flag
  }
}

void input_password(const char* label, char* buffer, int max_len) {
  if (custom_get_string(label, buffer, max_len, true) == -1) {
    // SIGINT was received during input, auth_ui_main will check global flag
  }
}

int auth_ui_main(char* logged_in_user) {
  char id[MAX_ID_LEN], pw[MAX_PW_LEN];
  int choice_char;
  int choice;

  logged_in_user[0] = '\0';  // Ensure logged_in_user is clean

  while (1) {
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

    draw_auth_menu();
    choice_char = getch();

    if (choice_char == ERR) {  // Timeout, no input
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
      continue;
    }

    if (choice_char >= '1' && choice_char <= '3') {
      choice = choice_char - '0';
    } else {
      choice = -1;  // Invalid
    }

    if (choice == 3) return AUTH_UI_EXIT_NORMAL;  // User chose 'Exit'
    if (choice != 1 && choice != 2) {
      mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Invalid option. Please try again.");
      clrtoeol();
      wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "");  // Just waits
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
      continue;
    }

    clear();
    mvprintw(Y_TITLE, X_DEFAULT_POS, (choice == 1) ? "=== LOGIN ===" : "=== REGISTER ===");

    input_plain("ID", id, MAX_ID_LEN);
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;  // Check after input_plain

    input_password("PW", pw, MAX_PW_LEN);
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;  // Check after input_password

    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "Processing...");
    clrtoeol();
    refresh();

    char response_message_buffer[MAX_MSG_LEN * 2];  // For combined messages

    if (choice == 1) {  // Login
      LoginResponse res;
      int ret = send_login_request(id, pw, &res);
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

      if (ret == 0 && res.success) {
        strncpy(logged_in_user, id, MAX_ID_LEN - 1);
        logged_in_user[MAX_ID_LEN - 1] = '\0';
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Login Success! %s", res.message);
        mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "%s", response_message_buffer);
        clrtoeol();
        wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key...");
        if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
        return AUTH_UI_LOGIN_SUCCESS;
      } else {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Login Failed: %s (ret: %d)",
                 (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    } else if (choice == 2) {  // Register
      RegisterResponse res;
      int ret = send_register_request(id, pw, &res);
      if (sigint_received) return AUTH_UI_EXIT_SIGNAL;

      if (ret == 0 && res.success) {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Register Success! %s Try logging in.", res.message);
      } else {
        snprintf(response_message_buffer, sizeof(response_message_buffer), "Register Failed: %s (ret: %d)",
                 (ret != 0 ? "Network/Comm error" : res.message), ret);
      }
    }

    mvprintw(Y_STATUS_MSG, X_DEFAULT_POS, "%s", response_message_buffer);
    clrtoeol();
    wait_for_key_or_signal(Y_STATUS_MSG + Y_MSG_OFFSET1, X_DEFAULT_POS, "Press any key to continue...");
    if (sigint_received) return AUTH_UI_EXIT_SIGNAL;
  }
}