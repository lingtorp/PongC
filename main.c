#include <ncurses.h>
#include <sys/time.h>
#include <stdlib.h>

// The number of milliseconds between ticks to the ball and opponent.
#define TICK_DELAY 50

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

// Gameplay handling
void tick_ball(pong_ball *ball, pong_win *opp, pong_win *player);
void tick_opponent(pong_win *opponent, pong_ball *ball);
void reset_ball(pong_ball *ball);

// Window handling
WINDOW *create_newwin(int height, int width, int starty, int startx);
void destroy_win(WINDOW *local_win);

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
  int rstartx = COLS - width;
  int lstartx = 0;

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
  ball.dyx = 1; // TODO: Random sign please
  ball.dyy = 1; // TODO: Random sign please
  ball.start_dyx = 1;
  ball.start_dyy = 1;
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

  /* Main loop */
  unsigned int diff;
  int ch;
  while (1) {
    // Check time
    gettimeofday(&now, NULL); // Deprecated in POSIX (?)
    diff = ((now.tv_sec * 1000) + (now.tv_usec / 1000)) -
           ((start.tv_sec * 1000) + (start.tv_usec / 1000));
    if (diff >= TICK_DELAY) {
      tick_ball(&ball, &opponent, &player);
      tick_opponent(&opponent, &ball);
      start = now;
    }

    // Process player input
    ch = getch();
    switch (ch) {
    case KEY_RIGHT:
      destroy_win(player.win);
      player.posY--;
      player.win = create_newwin(height, width, player.posY, player.posX);
      break;
    case KEY_LEFT:
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
    }

    // Redraw the tennis line
    mvvline(0, COLS / 2, '|', LINES);

    // Redraw instructions
    mvprintw(0, 0,
             "EXIT: CTRL + C \nRESET: r \n MOVEMENT: LEFT/RIGHT ARROW KEYS");

    // Redraw scores
    attron(A_BOLD); // The terminals best highlightning mode
    mvprintw(5, COLS / 3, "%i \n", player.score); // 1/3 of the width
    mvprintw(5, COLS - COLS / 3, "%i", opponent.score);
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

  // Add random velocity when collided with something
  if (ball->collided) {
    ball->dyx += rand() % 10;
    ball->dyy += rand() % 10;
    ball->collided = false;
  }

  // Update ball position by destroying and recreating the window
  destroy_win(ball->win.win);
  ball->win.posX += ball->dyx;
  ball->win.posY += ball->dyy;
  ball->win.win = create_newwin(ball->win.height, ball->win.width,
                                ball->win.posY, ball->win.posX);
}

// Makes the opponent follow the Y-coord of the ball, with a delay
void tick_opponent(pong_win *opponent, pong_ball *ball) {
  destroy_win(opponent->win);
  opponent->posY += (ball->win.posY - opponent->posY) / 10;
  opponent->win = create_newwin(opponent->height, opponent->width,
                                opponent->posY, opponent->posX);
}

// Resets the balls position to the middle of the screen
void reset_ball(pong_ball *ball) {
  ball->win.posX = COLS / 2 - 1;
  ball->win.posY = LINES / 2;
  ball->dyx = ball->start_dyx;
  ball->dyy = ball->start_dyy;
}

// Allocs a new window and sets a box around it plus displays it
WINDOW *create_newwin(int height, int width, int starty, int startx) {
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
