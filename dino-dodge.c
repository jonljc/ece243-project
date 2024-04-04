#include <stdbool.h>
#include <stdlib.h>

// ========== BOUNDARY DEFINITIONS ========== //
#define X_MAX (320 - 1)
#define Y_MAX (240 - 1)
#define GROUND_HEIGHT 5
#define Y_WORLD (Y_MAX - GROUND_HEIGHT)
#define JUMP_MIN (Y_WORLD - 200)

#define TIMER_ADDR 0xFF202000
#define TIMER_DELAY 100000000  // one second
#define HEX0_ADDR 0xFF200020
#define HEX4_ADDR 0xFF200030
#define LED_ADDR 0xFF200000
// END: Boundary Definitions

// ========== COLOUR DEFINITIONS ==========//
// Website for Colours https://chrishewett.com/blog/true-rgb565-colour-picker/
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define ORANGE 0xFBE0
#define YELLOW 0xFFE0
#define GREEN 0x7E0
#define CYAN 0x7FF
#define BLUE 0x1F
#define PINK 0xF81F

#define SKY_BLUE 0x975f
#define GRASS_GREEN 0x356a
// END: Colour Definitions

// ========== KEYBOARD DEFINITIONS ==========//
#define KEY_SPACE 0x29
#define KEY_L_ALT 0x11
#define KEY_ENTER 0x5A

// ========== OBJECT DATA STRUCTURES ========== //
struct Obstacle {
  bool collision;
  bool erase;
  bool go;
  bool cactus_obs_type;  // If obstacle type is a cactus, true.
                         // If obstacle type is a pterodactyl, false.
  int height;
  int width;
  int x_loc_prev;
  int y_loc_prev;
  int x_loc_cur;
  int y_loc_cur;
  short int colour;
};

struct Dino {
  bool airborne;
  bool rising;  // If dino is rising, true.
                // If dino is falling, false.
  bool erase;
  int x_loc_prev;
  int y_loc_prev;
  int x_loc_cur;
  int y_loc_cur;
  int height;
  int width;
  short int colour;
};
// END: Object Data Structures

// ========== HELPER FUNCTIONS PROTOTYPES ========== //
// Miscellaneous Prototypes
void swap(int* a, int* b);
int abs(int a);

// Draw Prototypes
void draw_line(int x0, int y0, int x1, int y1, short int color);
void plot_pixel(int x, int y, short int line_color);
void clear_screen();

void draw_dino(struct Dino my_dino);
void draw_single_cactus(int x0, int y0, short int color);
void draw_obstacle(struct Obstacle obs);
void draw_ground(short int color);

// De1-Soc Hardware Prototypes
void wait_for_vsync();
void init_timer();
void display_timer_HEX(int timer);
void use_LEDs(int num_lives);
void read_ps2_keyboard(unsigned char* pressed_key);

// Game Functionality Prototypes
void update_timer(int* timer);
void check_cactus_collision(struct Obstacle* cactus, struct Dino* trex, int* lives);

// END: Helper Function Prototypes

// ========== GLOBAL VARIABLES ========== //
int pixel_buffer_start;
int obstacle_speed = 2;
volatile int* timer_p = (int*)TIMER_ADDR;
short int Buffer1[240][512];  // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];
short int colour = 0xFFFF;
short int BACKGROUND_COL = SKY_BLUE;
int game_state = 2;  // 1 = home, 2 = main, 3 = game over
unsigned char pressed_key = 0;

int main(void) {
  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
  /* set front pixel buffer to Buffer 1 */
  *(pixel_ctrl_ptr + 1) =
      (int)&Buffer1;  // first store the address in the  back buffer
  /* now, swap the front/back buffers, to set the front buffer location */
  wait_for_vsync();  // polls until it is ready to write
  /* initialize a pointer to the pixel buffer, used by drawing functions */
  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();  // pixel_buffer_start points to the pixel buffer
  /* set back pixel buffer to Buffer 2 */
  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // we draw on the back buffer
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  if (game_state == 2) {
    struct Obstacle cactus0 = {false, false, false, true,         50,   20,
                               0,     0,     X_MAX, Y_WORLD - 50, BLACK};
    struct Obstacle cactus1 = {false, false, false, true,         50,    20,
                               0,     0,     X_MAX, Y_WORLD - 50, ORANGE};
    struct Obstacle cactus2 = {false, false,        false,      true,
                               50,    20,           0,          0,
                               X_MAX, Y_WORLD - 50, GRASS_GREEN};
    struct Obstacle pterodactyl0 = {false, false, false, false, 30,   30,
                                    0,     0,     X_MAX, 90,    BLACK};

    struct Obstacle Game_Obstacles[] = {cactus0, pterodactyl0, cactus1,
                                        cactus2};
    int num_obstacles = sizeof(Game_Obstacles) / sizeof(Game_Obstacles[0]);

    struct Dino trex = {false, true,          false, 0,  0,
                        20,    Y_WORLD - 120, 120,   49, PINK};

    init_timer();
    int timer = 0;
    int num_lives = 3;
    int cacti_dist;
    int go_dist;
    int cacti_buffer = 40;

    while (1) {
      update_timer(&timer);
      use_LEDs(num_lives);

      for (int i = 0; i < num_obstacles; i++) {
        /* Collision Detection */
        // ***ATTENTION NOTE: (trex.x_loc_cur + trex.width) needs to be (mod obstacle_speed - 1)
        // or else collision will be 1 pixel into the trex
        
        check_cactus_collision(&(Game_Obstacles[i]), &trex, &num_lives);
        
        /*
        if (Game_Obstacles[i].x_loc_cur <= trex.x_loc_cur + trex.width) {
          // If Game_Obstacles[i]
          if (Game_Obstacles[i].collision == false) {
            num_lives--;
          }
          Game_Obstacles[i].collision = true;
          use_LEDs(num_lives);
        }*/

        // New Obstacle Entering Screen
        if (i == 0) {  // In case we are handling first cactus
          Game_Obstacles[i].go = true;
        } else {
          cacti_dist =
              Game_Obstacles[i].x_loc_cur - Game_Obstacles[i - 1].x_loc_cur;
          go_dist = Game_Obstacles[i - 1].height + cacti_buffer;
          if (cacti_dist >= go_dist) {
            Game_Obstacles[i].go = true;
          }
        }

        /* Erase */
        Game_Obstacles[i].erase = true;
        draw_obstacle(Game_Obstacles[i]);

        /* Update Previous */
        Game_Obstacles[i].x_loc_prev = Game_Obstacles[i].x_loc_cur;
        Game_Obstacles[i].y_loc_prev = Game_Obstacles[i].y_loc_cur;

        /* Update Current */
        if (Game_Obstacles[i].collision == true) {
          if (Game_Obstacles[i].cactus_obs_type == true) {
            Game_Obstacles[i].y_loc_cur += obstacle_speed;
          } else if (Game_Obstacles[i].cactus_obs_type == false) {
            Game_Obstacles[i].y_loc_cur -= obstacle_speed;
          }

        } else if (Game_Obstacles[i].collision == false) {
          if (Game_Obstacles[i].go == true) {
            Game_Obstacles[i].x_loc_cur -= obstacle_speed;
          }
        }

        /* Draw */
        Game_Obstacles[i].erase = false;
        draw_obstacle(Game_Obstacles[i]);
        
      }

      if (trex.airborne == false) {
        read_ps2_keyboard(&pressed_key);
        if (pressed_key == KEY_SPACE) {
          trex.airborne = true;
        }
      }

      /* Erase */
      trex.erase = true;
      draw_dino(trex);

      /* Update Previous */
      trex.x_loc_prev = trex.x_loc_cur;
      trex.y_loc_prev = trex.y_loc_cur;

      /* Update Current */
      // (Update only if trex is airborne / in flight)
      if (trex.airborne == true) {
        // Update y_loc_cur
        if (trex.rising == true) {
          trex.y_loc_cur -= obstacle_speed;
        } else if (trex.rising == false) {
          trex.y_loc_cur += obstacle_speed;
        }

        // Set rising to false after reaching max height
        if (trex.y_loc_cur <= JUMP_MIN) {
          trex.rising = false;
        }
        // Set airborne to false after landing on ground
        // Set rising to true to initialize the next jump
        else if (trex.y_loc_cur + trex.height >= Y_WORLD) {
          trex.airborne = false;
          trex.rising = true;
        }
      }

      /* Draw */
      trex.erase = false;
      draw_dino(trex);

      /* Static Components */
      draw_ground(GRASS_GREEN);
      wait_for_vsync();  // swap front and back buffers on VGA vertical sync
      pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer

      if (num_lives <= 0) {
        break;
      }
    }
    timer = 0;
    display_timer_HEX(timer);
  }
}

void draw_obstacle(struct Obstacle obs) {
  int x_loc;
  int y_loc;
  if (obs.erase == true) {
    x_loc = obs.x_loc_prev;
    y_loc = obs.y_loc_prev;
    obs.colour = BACKGROUND_COL;
  } else {
    x_loc = obs.x_loc_cur;
    y_loc = obs.y_loc_cur;
  }

  if (x_loc <= X_MAX && x_loc >= 0 && y_loc <= Y_MAX && y_loc >= 0) {
    for (int i = obs.width; i > 0; i--) {
      draw_line(x_loc + i, y_loc + obs.height, x_loc + i, y_loc, obs.colour);
    }
  }
}

// Swap Function
void swap(int* a, int* b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

// Absolute Value Function
int abs(int a) { return a < 0 ? -a : a; }

// Draws Line on Screen
void draw_line(int x0, int y0, int x1, int y1, short int color) {
  bool is_steep = abs(y1 - y0) > abs(x1 - x0);

  if (is_steep) {
    swap(&x0, &y0);
    swap(&x1, &y1);
  }

  if (x0 > x1) {
    swap(&x0, &x1);
    swap(&y0, &y1);
  }

  int deltax = x1 - x0;
  int deltay = abs(y1 - y0);
  int error = -(deltax / 2);
  int y = y0;
  int y_step;

  if (y0 < y1) {
    y_step = 1;
  } else {
    y_step = -1;
  }

  for (int x = x0; x <= x1; ++x) {
    if (is_steep) {
      plot_pixel(y, x, color);
    } else {
      plot_pixel(x, y, color);
    }
    error = error + deltay;
    if (error > 0) {
      y = y + y_step;
      error = error - deltax;
    }
  }
}

// Plots the Pixel
void plot_pixel(int x, int y, short int line_color) {
  if (x >= (X_MAX + 1) || y >= (Y_MAX + 1)) {
    return;
  }

  short int* one_pixel_address;
  one_pixel_address = (short int*)(pixel_buffer_start + (y << 10) + (x << 1));
  *one_pixel_address = line_color;
}

// Clears Screen
void clear_screen() {
  for (int i = 0; i <= X_MAX; i++) {
    for (int j = 0; j <= Y_MAX; j++) {
      plot_pixel(i, j, BACKGROUND_COL);
    }
  }
}

// Delay Function using Polling Loop
void wait_for_vsync() {
  volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
  *pixel_ctrl_ptr = 1;  // pixel control pointer contains the value 1

  int status;
  status = *(
      pixel_ctrl_ptr +
      3);  // status register has the pixel control pointer address offset by 2

  while ((status & 1) != 0) {  // status & 1 masks to bit zero (s bit)
    status = *(pixel_ctrl_ptr +
               3);  // polling s bit until status is ready to write again
  }
}

// Draws Game Components
void draw_dino(struct Dino my_dino) {
  int x_loc;
  int y_loc;
  if (my_dino.erase == true) {
    x_loc = my_dino.x_loc_prev;
    y_loc = my_dino.y_loc_prev;
    my_dino.colour = BACKGROUND_COL;
  } else {
    x_loc = my_dino.x_loc_cur;
    y_loc = my_dino.y_loc_cur;
  }

  for (int i = 0; i < my_dino.height; i++) {
    draw_line(x_loc, y_loc + i, x_loc + my_dino.width, y_loc + i,
              my_dino.colour);
  }
}

void draw_ground(short int color) {
  int height = Y_MAX - GROUND_HEIGHT;
  for (int i = 0; i < GROUND_HEIGHT; i++) {
    draw_line(0, height + i, X_MAX, height + i, color);
  }
}

// Creates Timer
void init_timer() {
  // Initializing the Timer
  *timer_p = 0;        // TO = 0
  *(timer_p + 1) = 0;  // RUN=0

  *(timer_p + 2) =
      TIMER_DELAY &
      0xFFFF;  // writing lower 16 bits of TIMER_DELAY to offset 8 of timer
  *(timer_p + 3) =
      (TIMER_DELAY >> 16) &
      0xFFFF;  // writing upper 16 bits of TIMER_DELAY to offset 12 of timer

  *(timer_p + 1) = 0b110;  // start = 1, cont = 1 to begin the timer (110 where
                           // cont is bit 1 and start is bit 2)
}

void display_timer_HEX(int timer) {
  char to_7seg[16] = {~0b1000000, ~0b1111001, ~0b0100100,
                      ~0b0110000, ~0b0011001, ~0b0010010,
                      ~0b0000010, ~0b1111000, ~0b0000000,
                      ~0b0011000, ~0b0001000, ~0b0000011,
                      ~0b1000110, ~0b0100001, ~0b0000110,
                      ~0b0001110};  // using char because 7 bits

  volatile int* hex_p = (int*)HEX4_ADDR;
  *hex_p = 0;  // turns off HEX5 and HEX4

  int hex_data = 0;  // data that will be written to HEX0, HEX1, HEX2, HEX3
  // hex_data = (d3 << 24) | (d2 << 16) | (d1 << 8) | d0;

  for (int i = 0; i < 4;
       i++) {  // saying 4 because 3600 (1 hour) is 4 digits big

    int val = timer % 10;
    val = to_7seg[val] & 0xFF;  // converts val to a seven-seg value// get rid
                                // of & 0xFF if you manually invert

    val = val << (i * 8);  // shifts the value to put it in the proper place in
                           // hex_data (line 64)
    hex_data = hex_data | val;  // the value is or'ed to put into correct place

    timer = timer / 10;
  }
  // at this point, hex_data is ready to be returned to hex display

  hex_p = (int*)HEX0_ADDR;
  *hex_p = hex_data;
}

void use_LEDs(int num_lives) {
  //volatile int* led_p = (int*)LED_ADDR;
  if (num_lives == 1) {
    volatile int* led_p = (int*)LED_ADDR;
    *led_p = 0x0001;
  } else if (num_lives == 2) {
    volatile int* led_p = (int*)LED_ADDR;
    *led_p = 0x0003;
  } else if (num_lives == 3) {
    volatile int* led_p = (int*)LED_ADDR;
    *led_p = 0x0007;
  } else {
    volatile int* led_p = (int*)LED_ADDR;
    *led_p = 0x0000;
  }
}

// Extracts PS/2 pressed key using pointer to Address Map / port address
// Checks for read data valid before assigning make code for the pressed key
// to the int pointer variable given in input parameter
void read_ps2_keyboard(unsigned char* pressed_key) {
  volatile int* PS2_ptr = (int*)0xFF200100;  // PS/2 port address

  int PS2_data, R_VALID;

  PS2_data = *(PS2_ptr);          // read the Data register in the PS/2 port
  R_VALID = (PS2_data & 0x8000);  // mask bit-15 to check if read data valid
  
  // Only save PS2_data to *pressed_key if read data valid
  if (R_VALID != 0) {
    *(pressed_key) = (PS2_data & 0xFF); // if valid, mask the 8-bit make code
  }

  // Empty the rest of the FIFO queue
  while (R_VALID != 0) {
    PS2_data = *(PS2_ptr);
    R_VALID = (PS2_data & 0x8000);
  }

  // Our game will require 2 different keys to implement jump and slide
  // Make codes for different key Options:
  // W = 0x1D
  // S = 0x1B
  // SPACE = 0x29
  // L-ALT = 0x11
  // ENTER = 0x5A
}

// Polls timer device to see if TO; if yes then increment timer and call
// display_timer_HEX to update HEX display
void update_timer(int* timer) {
  if ((*timer_p & 1) == 1) {  // if timer expired (TO = 1)
    *timer_p = 0;             // TO = 0

    (*timer)++;

    display_timer_HEX(*timer);
  }
}


void check_cactus_collision(struct Obstacle* cactus, struct Dino* trex, int* lives) {
  // Check for y-bound collision, if cactus is within x-bound range of trex
  if ((cactus->x_loc_cur + cactus->width >= trex->x_loc_cur && cactus->x_loc_cur <= trex->x_loc_cur + trex->width) && (cactus->y_loc_cur <= trex->y_loc_cur + trex->height)) {
    if (cactus->collision == false) {
      (*lives)--;
    }
    cactus->collision = true;
    use_LEDs(*lives);
  }
}

