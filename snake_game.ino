#include <TFT_HX8357.h>
#include <EEPROM.h>

#define Centre 240

#define HEIGHT 320
#define WIDTH 480

#define Boxsize 16
#define Itemsize 12
#define SizeDiff int((Boxsize - Itemsize) / 2)
#define DefaultLength 5
#define BaseMultiplier 0.005
#define SprintFrame 3
#define FONT 1

TFT_HX8357 tft = TFT_HX8357();

enum Stage {
  Opening = 0,
  Idle = 1,
  Ready = 2,
  GameBG = 3,
  Ingame = 4,
  Gameover = 5,
};

enum Direction {
  Keep = 0,
  Up = 1,
  Down = 2,
  Left = 3,
  Right = 4,
};

class Coor {
  public:
    // initialize with x,y coordinates
    Coor() {}
    Coor(int x, int y)
      : X(x), Y(y) {}
    Coor(int x, int y, int w, int h)
      : X(x), Y(y), W(w), H(h) {}
    void set_coor(int x, int y) {
      X = x;
      Y = y;
    };
    int X = -1;
    int Y = -1;
    int H;
    int W;
};

// Pin Variables
const int BUTTONS[6] = {A0, A1, A2, A3, A4, A5};

// Program Variables
int score;
int snake_length;
int highscore = 0;
int sprint = 0;
int button = -1;
int difficulty = 0;
const int highscore_adr = 0;
long time_gamestart;
Stage stage;
Direction dir = Keep;
Coor food;
Coor snake[440];
Coor leaderboard(WIDTH - 8 * Boxsize + 10, 10, 8 * Boxsize - 20, HEIGHT - 20);

template <class T> int EEPROM_writeAnything(int ee, const T& value) {
 const byte* p = (const byte*)(const void*)&value;
 int i;
 for (i = 0; i < sizeof(value); i++)
 EEPROM.update(ee++, *p++);
 return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value) {
 byte* p = (byte*)(void*)&value;
 int i;
 for (i = 0; i < sizeof(value); i++)
 *p++ = EEPROM.read(ee++);
 return i;
}

int ButtonInput() {
  // no multiple input
  bool pressed[6] = {false};
  int count = 0;

  for (int i = 0; i < 6; i++){
    pressed[i] = (digitalRead(BUTTONS[i]) == LOW);
    if (pressed[i] == true){count++;}
  }
  if (count > 1) {
    Serial.println("MULTIPLE INPUT.");
    return -1;
  }
  else if (pressed[0]) {
    Serial.println("UP");
    return 0;
  }
  else if (pressed[1]) {
    Serial.println("DOWN");
    return 1;
  }
  else if (pressed[2]) {
    Serial.println("LEFT");
    return 2;
  }
  else if (pressed[3]) {
    Serial.println("RIGHT");
    return 3;
  }
  else if (pressed[4]) {
    Serial.println("A");
    return 4;
  }
  else if (pressed[5]) {
    Serial.println("B");
    return 5;
  }
  else {
    // Serial.println("NO INPUT.");
    return -1;
  }
};

void ChangeDirection() {
  // cannot change to oposite direction
  switch (button) {
    case 0:
      if (dir == Down) {
        break;
      }
      dir = Up;
      break;
    case 1:
      if (dir == Up) {
        break;
      }
      dir = Down;
      break;
    case 2:
      if (dir == Right) {
        break;
      }
      dir = Left;
      break;
    case 3:
      if (dir == Left) {
        break;
      }
      dir = Right;
      break;
    default:
      // dir = Keep;
      break;

  }
};

void ChangeDifficulty(int input) {
  // cannot change to oposite direction
  switch (input) {
    case 0:
      if (difficulty != 0) {
        difficulty--;
      }
      break;
    case 1:
      if (difficulty != 3) {
        difficulty++;
      }
      break;
    default:
      break;
  }
}

float SpeedWeight(int difficulty) {
  switch (difficulty) {
    case 0:
      return 1;
    case 1:
      return 1.3;
    case 2:
      return 1.5;
    case 3:
      return 2.0;
  }
};

Coor NextHead() {
  Coor temp;
  switch (dir) {
    case Up:
      temp.set_coor(snake[0].X, snake[0].Y - 1);
      break;
    case Down:
      temp.set_coor(snake[0].X, snake[0].Y + 1);
      break;
    case Left:
      temp.set_coor(snake[0].X - 1, snake[0].Y);
      break;
    case Right:
      temp.set_coor(snake[0].X + 1, snake[0].Y);
      break;
    default:
      temp.set_coor(snake[0].X, snake[0].Y);
      break;
  }
  return temp;
}

void SpawnFood() {
  bool valid = false;
  do {
    food.set_coor(random(0, 22), random(0, 20));
    for (int i = 0; i < snake_length; i++) {
      if (food.X == snake[i].X && food.Y == snake[i].Y) {
        valid = false;
      }
      else {
        valid = true;
        // Serial.println("Food Spawned!");
      }
    }
  } while (!valid);
}

void CheckCollision() {
  // self
  for (int i = 1; i < snake_length; i++) {
    if (snake[0].X == snake[i].X && snake[0].Y == snake[i].Y && dir != Keep) {
      stage = Gameover;
    }
  }
  // food
  if (snake[0].X == food.X && snake[0].Y == food.Y) {
    // increase length
    snake_length++;
    // score by food location type
    bool x_edge = (food.X == 0 || food.X == 22);
    bool y_edge = (food.Y == 0 || food.Y == 20);
    if (x_edge && y_edge) {
      score += 5;
    } else if (x_edge || y_edge) {
      score += 3;
    } else {
      score++;
    }
    // record highscore
    if (score > highscore) {
      highscore = score;
      EEPROM_writeAnything(highscore_adr, highscore);
    }
    // generate new food
    SpawnFood();
  }
  // wall
  if (snake[0].X < 0 || snake[0].X > 22 || snake[0].Y < 0 || snake[0].Y > 20) {
    stage = Gameover;
  }
}

void UpdateLeaderboard(int score, float speed) {
  // upper left corner of leaderboard
  char str_score[10];
  char str_highscore[10];
  char str_time[10];
  char str_tmp[10];
  char str_speed[10];
  double elapsed = millis() - time_gamestart;
  int min = int(elapsed / 60000);
  int sec = int((elapsed - 60000 * min) / 1000);

  sprintf(str_score, "%03d", score);
  sprintf(str_highscore, "%03d", highscore);
  sprintf(str_time, "%02d:%02d", min, sec);
  dtostrf(speed, 4, 3, str_tmp);
  sprintf(str_speed, "x%s", str_tmp);

  // Serial.println(str_speed);

  tft.fillRect(leaderboard.X + 20, leaderboard.Y + 20, leaderboard.W - 40, 280, TFT_LIGHTGREY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawCentreString("SCORE", int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 30, FONT);
  tft.drawCentreString(str_score, int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 60, FONT);
  tft.drawCentreString("HIGH", int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 100, FONT);
  tft.drawCentreString(str_highscore, int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 130, FONT);
  tft.drawCentreString("TIME", int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 170, FONT);
  tft.drawCentreString(str_time, int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 200, FONT);
  tft.drawCentreString("SPEED", int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 240, FONT);
  tft.drawCentreString(str_speed, int(leaderboard.X + leaderboard.W / 2), leaderboard.Y + 270, FONT);
};

void Menu() {
  int BgColor = TFT_DARKGREY;
  int TextColor = TFT_WHITE;
  long step = 0;

  tft.setTextColor(TextColor, BgColor);
  tft.setTextSize(4);
  if (stage == Opening) {
    tft.fillScreen(BgColor);
    for (int i = 240; i > 0; i -= 10) {
      tft.fillRect(Centre - 100, 80 + i, 200, 50, BgColor);
      tft.drawCentreString("SNAKE", Centre, 80 + i, FONT);
      delay(20);
    }
    tft.setTextSize(2);
    tft.drawCentreString("Press Any Keys.", Centre, 200, FONT);
    stage = Idle;
  }

  if (button == 4 && stage == Ready) {
    stage = GameBG;
  } else if (button != -1) {  // not empty
    // cover previous hint text
    tft.fillRect(Centre - 100, 150, 200, 150, BgColor);
    stage = Ready;
  }


  // Difficulties Select Animation
  if (stage == Ready) {

    tft.setTextSize(3);
    tft.drawCentreString("Easy", Centre, 160, FONT);
    tft.drawCentreString("Normal", Centre, 200, FONT);
    tft.drawCentreString("Hard", Centre, 240, FONT);
    tft.drawCentreString("Extreme", Centre, 280, FONT);

    if (step % 2) {
      tft.setTextColor(TextColor, BgColor);
    } else {
      tft.setTextColor(TextColor, TFT_LIGHTGREY);
    }
    ChangeDifficulty(button);
    switch (difficulty) {
      case 0:
        tft.drawCentreString("Easy", Centre, 160, FONT);
        break;
      case 1:
        tft.drawCentreString("Normal", Centre, 200, FONT);
        break;
      case 2:
        tft.drawCentreString("Hard", Centre, 240, FONT);
        break;
      case 3:
        tft.drawCentreString("Extreme", Centre, 280, FONT);
        break;
    }
    delay(100);
    step += 1;
  }
};

void DrawGameBackground() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < WIDTH - 8 * Boxsize; i += Boxsize) {
    for (int j = 0; j < HEIGHT; j += Boxsize) {
      tft.drawRect(i, j, Boxsize, Boxsize, TFT_DARKGREY);
    }
  }
  UpdateLeaderboard(score, 1.0);
};

void Game() {
  int BgColor = TFT_BLACK;
  if (dir == Keep) { time_gamestart = millis(); }

  if (stage == GameBG) {
    // initialize snake
    // Head at random coordinate on 22x20 grid
    snake[0].X = random(0, 22);
    snake[0].Y = random(0, 20);

    for (int i = 1; i < snake_length; i++) {
      snake[i] = snake[i - 1];
    }

    // first food
    SpawnFood();

    // Draw Background
    DrawGameBackground();

    tft.fillRoundRect(leaderboard.X, leaderboard.Y, leaderboard.W, leaderboard.H, 5, TFT_LIGHTGREY);

    stage = Ingame;
  }
  else {
    Coor next_head;

    ChangeDirection();
    // get next snake head coordinate
    next_head = NextHead();

    // draw snake
    // 1. clear previous snake, head move to new coordinate
    for (int i = snake_length - 1; i >= 0; i--) { // N-1 to 0
      tft.fillRoundRect(snake[i].X * Boxsize + SizeDiff, snake[i].Y * Boxsize + SizeDiff, Itemsize, Itemsize, 3, BgColor);
      if (i > 0) { snake[i] = snake[i - 1]; }
    }
    snake[0] = next_head;

    // 2. draw current snake
    for (int i = 0; i < snake_length; i++) {
      if (snake[i].X == -1 && snake[i].Y == -1) { snake[i] = snake[i - 1]; }
      tft.fillRoundRect(snake[i].X * Boxsize + SizeDiff, snake[i].Y * Boxsize + SizeDiff, Itemsize, Itemsize, 3, TFT_DARKGREEN);
    }
    delay(20);
    // check self/food/wall collision
    CheckCollision();

    // draw food
    tft.fillRoundRect(food.X * Boxsize + SizeDiff, food.Y * Boxsize + SizeDiff, Itemsize, Itemsize, 3, TFT_ORANGE);

    // speedup with score
    int wait = 200;
    float weight = SpeedWeight(difficulty);
    float speed;
    
    // A button sprint
    if (button == 4){
      Serial.println("SPRINT!");
      sprint = SprintFrame;
    }

    if (sprint > 0) {
      speed = min(1 + (BaseMultiplier * weight * score * 2), 10);
      sprint--;
    }
    else {
      speed = min(1 + (BaseMultiplier * weight * score), 10);
    }

    // update leaderboard
    UpdateLeaderboard(score, speed);
    
    delay(int(wait / speed));
    
  }
};

void GameOver() {
  tft.fillRoundRect(0, 160 - 50, 480, 100, 10, TFT_DARKGREY);
  tft.setTextSize(6);
  tft.setTextColor(TFT_RED);
  tft.drawCentreString("Game Over!", Centre, 120, FONT);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString("Press \"B\" To Restart.", Centre, 180, FONT);

  delay(500);

  if (button == 5) {
    // reset variables
    score = 0;
    snake_length = DefaultLength;
    dir = Keep;
    stage = Opening;
  }
}

void setup() {
  // Fix random seed, comment when testing
  // randomSeed(analogRead(0));
  Serial.begin(38400);

  // Setup the LCD
  tft.init();
  // 1: USB right, 3. USB left
  tft.setRotation(1);

  // Initialize Variables
  score = 0;
  // EEPROM_writeAnything(highscore_adr, 0);
  EEPROM_readAnything(highscore_adr, highscore);
  snake_length = DefaultLength;
  // skip stage
  stage = Opening;
  // stage = GameBG;

  // pins setup
  for (int i = 0; i < 6; i++) {
    pinMode(BUTTONS[i], INPUT_PULLUP);
  }
  Serial.println("Arduino Started.");
}

void loop() {
  // Serial.println(highscore);
  button = ButtonInput();
  switch (stage) {
    case Opening: case Idle: case Ready:
      Menu();
      break;

    case GameBG: case Ingame:
      Game();
      break;

    case Gameover:
      GameOver();
      break;

    default:
      Serial.println("Invalid Stage.");
      break;
  }
}