#include <ncurses.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// The number of milliseconds between ticks to the ball and opponent.
#define TICK_DELAY 40

typedef struct {
  int score;
  int posX, posY;
  int height, width;
  WINDOW *win;
} pong_win;

typedef struct {
  pong_win win;
  int dyx;
  int dyy;
  int start_dyx;
  int start_dyy;
  bool collided;
} pong_ball;

typedef struct {
  int padding;
} config;

// Gameplay handling
void tick_ball(pong_ball *ball, pong_win *opp, pong_win *player);
void tick_opponent(pong_win *opponent, const pong_ball *ball);
void reset_ball(pong_ball *ball);

// User inteface
void draw_score(const uint32_t x, const uint32_t y, const uint8_t score);
void display_start_menu(config *cfg);

// Window handling
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

int rand_sign();
int sign(const int x);

int main() {
  srand(time(NULL));
  WINDOW *root = initscr(); /* initialize the curses library */

  cbreak();             /* Line buffering disabled pass on everything to me*/
  keypad(stdscr, TRUE); /* For keyboard arrows 	*/
  noecho();             /* Do not echo out input */
  nodelay(root, true);  /* Make getch non-blocking */
  refresh();

  int height = 10;                    // Box height
  int width = 2;                      // Box width
  int lstarty = (LINES - height) / 2; // Midheight of the window, start for pads
  int rstarty = lstarty;
  int lstartx = 2;
  int rstartx = COLS - width - lstartx;

  // Init the opponent
  pong_win opponent;
  WINDOW *opponent_win = create_newwin(height, width, rstarty, rstartx);
  opponent.win = opponent_win;
  opponent.height = height;
  opponent.width = width;
  opponent.posX = rstartx;
  opponent.posY = rstarty;
  opponent.score = 0;

  // Init the player
  WINDOW *player_win = create_newwin(height, width, lstarty, lstartx);
  pong_win player;
  player.win = player_win;
  player.height = height;
  player.width = width;
  player.posX = lstartx;
  player.posY = lstarty;
  player.score = 0;

  // Init the ball
  pong_ball ball;
  ball.dyx = rand_sign();
  ball.dyy = rand_sign();
  ball.collided = false;
  pong_win ball_win;
  ball_win.height = 2;
  ball_win.width = 3;
  ball_win.posX = COLS / 2 - ball_win.width / 2;
  ball_win.posY = LINES / 2;
  ball_win.win = create_newwin(2, 3, ball_win.posY, ball_win.posX);
  ball.win = ball_win;

  // Init the time
  struct timeval start; // tv_sec = seconds, tv_usec = nanoseconds
  struct timeval now;
  gettimeofday(&start, NULL);

  config cfg;
  display_start_menu(&cfg);

  // Main loop
  int ch = 0;
  bool done = false;
  while (!done) {

    // Process player input
    ch = getch();
    switch (ch) {
    case 'k':
      destroy_win(player.win);
      player.posY--;
      player.win = create_newwin(height, width, player.posY, player.posX);
      break;
    case 'j':
      destroy_win(player.win);
      player.posY++;
      player.win = create_newwin(height, width, player.posY, player.posX);
      break;
    case 'r': // Reset the game
      destroy_win(player.win);
      player.posX = lstartx;
      player.posY = lstarty;
      player.win = create_newwin(height, width, player.posY, player.posX);
      player.score = 0;
      opponent.score = 0;
      reset_ball(&ball);
      break;
    case 'q':
      done = true;
    }

    // Redraw instructions
    mvprintw(0, 0, "EXIT: q \nRESET: r\nMOVEMENT: UP: k, DOWN: j");

    // Redraw scores
    draw_score(5, COLS / 3, player.score);
    draw_score(5, COLS - COLS / 3, opponent.score);

    // Redraw the tennis line
    mvvline(0, COLS / 2, '|', LINES);

    // Check time
    gettimeofday(&now, NULL); // Deprecated in POSIX (?)
    unsigned int diff = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) -
                        ((start.tv_sec * 1000) + (start.tv_usec / 1000));

    // Draw ball
    if (diff >= TICK_DELAY) {
      tick_ball(&ball, &opponent, &player);
      tick_opponent(&opponent, &ball);
      start = now;
    }

    standend(); // Reset all attributes
  }

  endwin(); /* End curses mode */
  return 0;
}

// Uses the bounds of the terminal window
void tick_ball(pong_ball *ball, pong_win *opp, pong_win *player) {
  // Check root window bounds - top and bot
  if (ball->win.posY + ball->win.height >= LINES || ball->win.posY <= 0) {
    ball->dyy = ball->dyy * -1; // Flip the sign (bounce back)
  }

  // Check root window bounds - left and right
  if (ball->win.posX + ball->win.width >= COLS) {
    // The ball missed the pads and hit the wall; award the right player
    player->score++;
    reset_ball(ball);
  } else if (ball->win.posX <= 0) {
    opp->score++;
    reset_ball(ball);
  }

  // Check collision with Player
  if (ball->win.posY >= player->posY &&
      ball->win.posY <= player->posY + player->height &&
      ball->win.posX <= player->posX + player->width) {
    ball->dyx = ball->dyx * -1; // Flip the sign (bounce back)
    ball->collided = true;
  }

  // Check collision with Opponent (aka opp)
  if (ball->win.posY >= opp->posY &&
      ball->win.posY <= opp->posY + opp->height &&
      ball->win.posX >= opp->posX - opp->width) {
    ball->dyx = ball->dyx * -1; // Flip the sign (bounce back)
    ball->collided = true;
  }

  // Add random velocity when collided with players
  if (ball->collided) {
    ball->dyx += sign(ball->dyx) * rand() % 3;
    ball->dyy += sign(ball->dyy) * rand() % 2;
    ball->collided = false;

    // Redraw both players
    destroy_win(player->win);
    player->win = create_newwin(player->height, player->width, player->posY,
                                player->posX);
    destroy_win(opp->win);
    opp->win = create_newwin(opp->height, opp->width, opp->posY, opp->posX);
  }

  // Update ball position by destroying and recreating the window
  destroy_win(ball->win.win);
  ball->win.posX += ball->dyx;
  ball->win.posY += ball->dyy;
  ball->win.win = create_newwin(ball->win.height, ball->win.width,
                                ball->win.posY, ball->win.posX);
}

// Makes the opponent follow the Y-coord of the ball, with a delay
void tick_opponent(pong_win *opponent, const pong_ball *ball) {
  destroy_win(opponent->win);
  opponent->posY += (ball->win.posY - opponent->posY) / 10;
  opponent->win = create_newwin(opponent->height, opponent->width,
                                opponent->posY, opponent->posX);
}

// Resets the balls position to the middle of the screen
void reset_ball(pong_ball *ball) {
  ball->win.posX = COLS / 2 - 1;
  ball->win.posY = LINES / 2;
  ball->dyx = rand_sign();
  ball->dyy = rand_sign();
}

// Allocs a new window and sets a box around it plus displays it
WINDOW *create_newwin(const int height, const int width, const int starty,
                      const int startx) {
  WINDOW *local_win;

  local_win = newwin(height, width, starty, startx);
  box(local_win, 0, 0); /* 0, 0 gives default characters
                         * for the vertical and horizontal
                         * lines			*/
  wrefresh(local_win);  /* Show that box 		*/

  return local_win;
}

// Deallocs the window and removes leftover artefacts
void destroy_win(WINDOW *local_win) {
  /* box(local_win, ' ', ' '); : This won't produce the desired
   * result of erasing the window. It will leave it's four corners
   * and so an ugly remnant of window.
   */
  wborder(local_win, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
  wrefresh(local_win);
  delwin(local_win);
}

// Return -1/1 with a roughly 50% chance
int rand_sign() {
  return (float)rand() / ((float)INT32_MAX / 2.0f) <= 0.5 ? -1 : 1;
}

// Returns -1/0/1 depending on the x being neg., zero or pos.
int sign(const int x) {
  if (x == 0) {
    return 0;
  }
  return x > 0 ? 1 : -1;
}

void draw_score(const uint32_t y, const uint32_t x, const uint8_t score) {
  // TODO: Larger numbers as the original
  attron(A_BOLD);
  mvprintw(y, x, "%i", score);
}

#define SERVER_PORT 8080
void server() {
  // Init TCP socket
  int server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket_fd == 0) {
    fprintf(stderr, "Failed to create server socket");
    exit(-1);
  }

  struct sockaddr_in addr;
  addr.sin_family = AF_UNSPEC; // IPv4, IPv6, w/e
  addr.sin_port = htons(SERVER_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(server_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    fprintf(stderr, "Failed to bind socket");
    exit(-1);
  }

  const uint32_t BACKLOG = 3;
  if (listen(server_socket_fd, BACKLOG) != 0) {
    fprintf(stderr, "Failed to listen");
  }

  bool done = false;
  while (!done) {
    struct sockaddr_storage client_socket;
    uint32_t addr_size = sizeof(struct sockaddr_storage);
    int client_socket_fd = accept(server_socket_fd, (struct sockaddr *)&client_socket, &addr_size);
    if (client_socket_fd < 0) {
      fprintf(stderr, "Failed to accept");
    } else {
      // Send msg to client that connected
      const char* msg = "PongC server response";
      const int lng = sizeof(msg);
      const int bytes_sent = send(client_socket_fd, msg, lng, 0);
      if (bytes_sent < lng) {
        fprintf(stderr, "Did not successfully send full msg");
      }
      close(client_socket_fd);
    }
  }
}

void client() {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    fprintf(stderr, "Failed to create client socket");
  }

  struct sockaddr_in server;
  // TODO: Broadcast later on, hardcode IP for now?
}

void display_start_menu(config *cfg) {
  // TODO: Choose AI or multiplayer over network or local
  if (cfg) {
    move(0, 0);
  }

  mvprintw(0, 0, "START MENU");

  bool done = false;
  while (!done) {

    // Process player input
    const char ch = getch();
    switch (ch) {
    case 's':
      mvprintw(0, 0, "Starting server on: ");
      server();
      break;
    case 'c':
      mvprintw(0, 0, "Starting client on: ");
      client();
      break;
    case 'q':
      done = true;
      break;
    }
  }
}
