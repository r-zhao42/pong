#include "raylib.h"
#include "stdio.h"
#include <stdlib.h>

typedef struct {
  int width;
  int height;
  int x;
  int y;
  int vx;
  int vy;
} CollisionObject;

typedef enum { START, PAUSE, GAMEPLAY, GAMEOVER } GameScreen;

int max(int a, int b) { return a > b ? a : b; }

int min(int a, int b) { return a < b ? a : b; }

int overlapArea(CollisionObject a, CollisionObject b) {
  int left = max(a.x, b.x);
  int top = max(a.y, b.y);
  int right = min(a.x + a.width, b.x + b.width);
  int bottom = min(a.y + a.height, b.y + b.height);

  int width = right - left;
  int height = bottom - top;
  return max(height * width, 0);
}

bool isAABBColliding(CollisionObject a, CollisionObject b) {
  int a_left = a.x;
  int a_right = a.x + a.width;
  int a_top = a.y;
  int a_bottom = a.y + a.height;

  int b_left = b.x;
  int b_right = b.x + b.width;
  int b_top = b.y;
  int b_bottom = b.y + b.height;

  if (a_right > b_left && a_left < b_right && a_bottom > b_top &&
      a_top < b_bottom) {
    return true;
  };
  return false;
};

// checks collision with object B as the anchor.
// Returns which edge of the anchor (b) a collision occured on, or -1 if no
// collision occurred
// 0 = top
// 1 = right
// 2 = bottom
// 3 = left
int edgeCollision(CollisionObject a, CollisionObject b) {
  CollisionObject b_left = {1, b.height, b.x, b.y, 0, 0};
  CollisionObject b_right = {1, b.height, b.x + b.width, b.y, 0, 0};
  CollisionObject b_top = {b.width, 1, b.x, b.y, 0, 0};
  CollisionObject b_bottom = {b.width, 1, b.x, b.y + b.height, 0, 0};

  if (isAABBColliding(a, b)) {
    int left = overlapArea(b_left, a);
    int right = overlapArea(b_right, a);
    int top = overlapArea(b_top, a);
    int bottom = overlapArea(b_bottom, a);

    int collisionDirectionObjects[4] = {top, right, bottom, left};
    int collisionDirectionIndex = 0;
    int maxOverlapArea = 0;

    for (int i = 0; i < 4; i++) {
      if (collisionDirectionObjects[i] > maxOverlapArea) {
        maxOverlapArea = collisionDirectionObjects[i];
        collisionDirectionIndex = i;
      }
    }
    return collisionDirectionIndex;
  }
  return -1;
}

void checkBallCollision(CollisionObject *object, CollisionObject *ball,
                        Sound hitSound) {
  int collisionDirection = edgeCollision(*object, *ball);

  if (collisionDirection == -1) {
    return;
  }
  PlaySound(hitSound);

  switch (collisionDirection) {
  case 0:
    ball->vy = -ball->vy;
    // Premptively set x/y position to avoid bug where collision is double
    // calculated between frames, causing ball to phase through paddle
    ball->y = object->y + object->height;
    break;
  case 1:
    ball->vx = -ball->vx;
    ball->x = object->x - ball->width;
    break;
  case 2:
    ball->vy = -ball->vy;
    ball->y = object->y - ball->height;
    break;
  case 3:
    ball->vx = -ball->vx;
    ball->x = object->x + object->width;
    break;
  }
}

void resetBall(CollisionObject *ball, int pos_x, int pos_y) {
  ball->x = pos_x;
  ball->y = pos_y;

  int x_speed_idx = (rand() % 2);
  int y_speed_index = (rand() % 2);
  int speeds[2] = {500, -500};

  int rand_vx = speeds[x_speed_idx];
  int rand_vy = speeds[y_speed_index];

  ball->vx = rand_vx;
  ball->vy = rand_vy;
}

// Implementation and usage of this gamestate is pretty rough, was just using
// stack variables to control game state but wanted to use a struct so callbacks
// to buttons would have access to update everything if needed Wasn't really
// necessary though, so didn't go through the effort of cleaning up everything
// to use this. Probably could have just used some global variables lol 
// TODO: clean this up a bit
typedef struct {
  CollisionObject *paddle1;
  CollisionObject *paddle2;
  CollisionObject *ball;
  int player1Score;
  int player2Score;
  GameScreen screen;
} GameState;

typedef struct {
  char *text;
  int centerX;
  int centerY;
  Color backgroundColor;
  Color foregroundColor;
  void (*f)(GameState *);
  GameState *gameStateContext;
} ButtonConfig;

void setupButton(ButtonConfig config) {
  char *buttonText = config.text;
  Vector2 textSize = MeasureTextEx(GetFontDefault(), buttonText, 20.0, 1.0f);
  int padding = 5;

  int rectWidth = textSize.x + padding * 4;
  int rectHeight = textSize.y + padding * 2;

  int rectX = config.centerX - rectWidth / 2;
  int rectY = config.centerY - rectHeight / 2;
  int textX = rectX + padding;
  int textY = rectY + padding;

  Vector2 mousePos = GetMousePosition();
  Color textColor;
  Color buttonBackgroundColor;
  if (mousePos.x > rectX && mousePos.x < rectX + rectWidth &&
      mousePos.y > rectY && mousePos.y < rectY + rectHeight) {
    textColor = config.backgroundColor;
    buttonBackgroundColor = config.foregroundColor;
  } else {
    textColor = config.foregroundColor;
    buttonBackgroundColor = config.backgroundColor;
  }

  if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && mousePos.x > rectX &&
      mousePos.x < rectX + rectWidth && mousePos.y > rectY &&
      mousePos.y < rectY + rectHeight) {
    (*(config.f))(config.gameStateContext);
  }

  DrawRectangle(rectX, rectY, rectWidth, rectHeight, buttonBackgroundColor);
  DrawRectangleLines(rectX, rectY, rectWidth, rectHeight,
                     config.foregroundColor);
  DrawText(buttonText, textX, textY, 20, textColor);
}

void setGameplayState(GameState *gameStateContext) {
  gameStateContext->screen = GAMEPLAY;
}

void resetAndRestart(GameState *gameStateContext) {
  gameStateContext->screen = GAMEPLAY;
  gameStateContext->player1Score = 0;
  gameStateContext->player2Score = 0;
  resetBall(gameStateContext->ball, GetScreenWidth() / 2,
            GetScreenHeight() / 2);
}

int main(void) {
  bool isPaused = false;

  // TODO: make game work for all screen sizes. Would have to decide how to
  // handle sizes of everything
  // 1. Relative to screen size
  // 2. Keep an absolute size
  int screenWidth = 800;
  int screenHeight = 450;
  Color background_color = BLACK;
  Color foreground_color = LIGHTGRAY;

  InitWindow(screenWidth, screenHeight, "raylib example - basic window");
  InitAudioDevice();
  int currentFps = 60;
  SetTargetFPS(currentFps);

  int ball_start_x = screenWidth / 2;
  int ball_start_y = screenHeight / 2;

  CollisionObject ball = {.width = 20,
                          .height = 20,
                          .x = ball_start_x,
                          .y = ball_start_y,
                          .vx = -200,
                          .vy = 200};

  resetBall(&ball, screenWidth / 2, screenHeight / 2);

  CollisionObject paddle1 = {.width = 10,
                             .height = 100,
                             .x = 50,
                             .y = screenHeight / 2,
                             .vx = 200,
                             .vy = 400};

  CollisionObject paddle2 = {.width = 10,
                             .height = 100,
                             .x = screenWidth - 50,
                             .y = screenHeight / 2,
                             .vx = 200,
                             .vy = 400};

  CollisionObject topBoundary = {
      .width = screenWidth, .height = 20, .x = 0, .y = 0, .vx = 0, .vy = 0};

  CollisionObject bottomBoundary = {.width = screenWidth,
                                    .height = 20,
                                    .x = 0,
                                    .y = screenHeight,
                                    .vx = 0,
                                    .vy = 0};

  CollisionObject *collisionObjects[] = {&paddle1, &paddle2, &topBoundary,
                                         &bottomBoundary};

  // Setup obstacles
  // Very rough prototype to test out that adding arbitrary collision objects
  // works with the simple collision system I implemented
  // TODO: Can probably clean this up, would like to just use a single
  // collisionObjects array, but not sure how to make it dynamic
  CollisionObject obstacles[25];
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      CollisionObject obstacle = {
          15, 15, (screenWidth / 5) * j + 70, (screenHeight / 5) * i + 40,
          0,  0};

      obstacles[i * 5 + j] = obstacle;
    }
  }

  Sound hitSound = LoadSound("assets/hit.wav");
  if (hitSound.frameCount == 0) {
    fprintf(stderr, "ERROR: Unable to load \"hit.wav\". Make sure you have the "
                    "audio file available");
  }

  char player1_score_text[3] = "0";
  char player2_score_text[3] = "0";

  GameState gameState;
  gameState.ball = &ball;
  gameState.paddle1 = &paddle1;
  gameState.paddle2 = &paddle2;
  gameState.player1Score = 0;
  gameState.player2Score = 0;
  gameState.screen = START;

  while (!WindowShouldClose()) {

    BeginDrawing();
    ClearBackground(background_color);

    switch (gameState.screen) {
    case START:
      if (IsKeyPressed(KEY_I)) {
        gameState.screen = GAMEPLAY;
      }

      ButtonConfig startButtonConfig = {.text = "START_GAME",
                                        .centerX = screenWidth / 2,
                                        .centerY = 100,
                                        .backgroundColor = background_color,
                                        .foregroundColor = foreground_color,
                                        .f = setGameplayState,
                                        .gameStateContext = &gameState};

      setupButton(startButtonConfig);
      DrawText(player1_score_text, screenWidth / 2 - 50, 25, 20,
               foreground_color);
      DrawText(player2_score_text, screenWidth / 2 + 50, 25, 20,
               foreground_color);
      DrawRectangle(paddle1.x, paddle1.y, paddle1.width, paddle1.height,
                    foreground_color);
      DrawRectangle(paddle2.x, paddle2.y, paddle2.width, paddle2.height,
                    foreground_color);
      break;
    case GAMEPLAY:
      if (IsKeyPressed(KEY_P)) {
        gameState.screen = PAUSE;
      }
      if (IsKeyPressed(KEY_R)) {
        resetBall(&ball, screenWidth / 2, screenHeight / 2);
        gameState.player1Score = 0;
        gameState.player2Score = 0;
      }

      if (gameState.player1Score > 5 || gameState.player2Score > 5) {
        gameState.screen = GAMEOVER;
      }

      float dt = GetFrameTime();

      // Player 1 input
      if (IsKeyDown(KEY_DOWN) && paddle1.y + paddle1.height <= screenHeight) {
        paddle1.y += dt * paddle1.vy;
      };
      if (IsKeyDown(KEY_UP) && paddle1.y >= 0) {
        paddle1.y -= dt * paddle1.vy;
      };

      // Player 2 input
      if (IsKeyDown(KEY_S) && paddle2.y + paddle2.height <= screenHeight) {
        paddle2.y += dt * paddle2.vy;
      };
      if (IsKeyDown(KEY_W) && paddle2.y >= 0) {
        paddle2.y -= dt * paddle2.vy;
      };

      // Collisions

      // TODO: I think I could clean up the collision logic considerably by
      // leveraging the new gamestate struct and adding a function/collision
      // effect argument that is a pointer to a function in the checkCollision
      // function
      for (int i = 0; i < sizeof(collisionObjects) / sizeof(CollisionObject *);
           i++) {
        checkBallCollision(collisionObjects[i], &ball, hitSound);
      }

      for (int i = 0; i < sizeof(obstacles) / sizeof(obstacles[0]); i++) {
        checkBallCollision(&(obstacles[i]), &ball, hitSound);
      }

      if (ball.x + ball.width > screenWidth) {
        gameState.player1Score += 1;
        ball.x = ball_start_x;
        ball.y = ball_start_y;
        resetBall(&ball, screenWidth / 2, screenHeight / 2);
      }

      if (ball.x < 0) {
        gameState.player2Score += 1;
        ball.x = ball_start_x;
        ball.y = ball_start_y;
        resetBall(&ball, screenWidth / 2, screenHeight / 2);
      }

      float ball_dx = dt * ball.vx;
      float ball_dy = dt * ball.vy;
      ball.x += ball_dx;
      ball.y += ball_dy;

      snprintf(player1_score_text, 3, "%d", gameState.player1Score);
      snprintf(player2_score_text, 3, "%d", gameState.player2Score);

      // -- DRAW Screen

      DrawText(player1_score_text, screenWidth / 2 - 50, 25, 20,
               foreground_color);
      DrawText(player2_score_text, screenWidth / 2 + 50, 25, 20,
               foreground_color);

      DrawRectangle(ball.x, ball.y, ball.width, ball.height, foreground_color);
      DrawRectangle(paddle1.x, paddle1.y, paddle1.width, paddle1.height,
                    foreground_color);
      DrawRectangle(paddle2.x, paddle2.y, paddle2.width, paddle2.height,
                    foreground_color);
      for (int i = 0; i < sizeof(obstacles) / sizeof(obstacles[0]); i++) {
        DrawRectangle(obstacles[i].x, obstacles[i].y, obstacles[i].width,
                      obstacles[i].height, foreground_color);
      }
      break;
    case PAUSE:
      if (IsKeyPressed(KEY_P)) {
        gameState.screen = GAMEPLAY;
      }
      DrawText("Paused", screenWidth / 2 + 50, 25, 20, foreground_color);

      DrawRectangle(ball.x, ball.y, ball.width, ball.height, foreground_color);
      DrawRectangle(paddle1.x, paddle1.y, paddle1.width, paddle1.height,
                    foreground_color);
      DrawRectangle(paddle2.x, paddle2.y, paddle2.width, paddle2.height,
                    foreground_color);
      break;
    case GAMEOVER:
      DrawText("Game Over", screenWidth / 2 - MeasureText("Game Over", 20) / 2,
               25, 20, foreground_color);

      ButtonConfig gameOverButtonConfig = {.text = "Play Again?",
                                           .centerX = screenWidth / 2,
                                           .centerY = screenHeight / 2,
                                           .backgroundColor = background_color,
                                           .foregroundColor = foreground_color,
                                           .f = resetAndRestart,
                                           .gameStateContext = &gameState};

      setupButton(gameOverButtonConfig);
      char *gameoverText;
      if (gameState.player1Score > gameState.player2Score) {
        gameoverText = "Player 1 won!";
      } else if (gameState.player1Score < gameState.player2Score) {
        gameoverText = "Player 2 won!";
      } else {
        gameoverText = "Draw!";
      }
      DrawText(gameoverText,
               screenWidth / 2 - MeasureText(gameoverText, 20) / 2, 100, 20,
               foreground_color);

      DrawRectangle(paddle1.x, paddle1.y, paddle1.width, paddle1.height,
                    foreground_color);
      DrawRectangle(paddle2.x, paddle2.y, paddle2.width, paddle2.height,
                    foreground_color);
      break;
    }

    EndDrawing();
  }

  UnloadSound(hitSound);
  CloseAudioDevice();
  CloseWindow();

  return 0;
}
