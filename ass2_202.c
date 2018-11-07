#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <avr/io.h> 
#include <avr/interrupt.h>
#include <util/delay.h>
#include <cpu_speed.h>
#include <sprite.h>
#include <graphics.h>
#include <macros.h>
#include "lcd_model.h"
#include "cab202_adc.h"
#include "usb_serial.h"

#define INIT_LIVES (2)
#define FREQ     (8000000.0)
#define TIMER_SCALE (256.0)
#define PRESCALE (1024.0)
#define PLAT_WIDTH (10)
#define COL_WIDTH (PLAT_WIDTH + 10)
#define ROW_HEIGHT (PLAT_HEIGHT + 8 + 2)
#define PLAT_HEIGHT (2)
#define PLAYER_WIDTH (8)
#define PLAYER_HEIGHT (8)
#define TREASURE_WIDTH (6)
#define TREASURE_HEIGHT (5)
#define OVERFLOW_TOP (1023)
#define ADC_MAX (1023)
#define BIT(x) (1 << (x))
#define NUM_FOOD (5)
#define NUM_ZOMBIES (5)
#define ZOMBIE_WIDTH (5)
#define ZOMBIE_HEIGHT (5)
#define FOOD_WIDTH (3)
#define FOOD_HEIGHT (2)

Sprite platform[30];
Sprite zombies[NUM_ZOMBIES];
Sprite food[NUM_FOOD];
Sprite player;
Sprite treasure;
char buffer[40];
typedef enum { false, true } bool;
bool start_game = false;
bool game_over = false;
bool is_safe[30];
double start_time;
int lives;
int score;
int num_plats;
int num_food_inventory;
int num_zombies;
uint8_t player_death_direct[8];

uint8_t block_image[] = { 	
	0b11111111, 	0b11111111, 
	0b11111111,     0b11111111,
};

uint8_t forb_image[] = { 
	0b10010010, 	0b01001001, 
	0b10010010, 	0b01001001, 
};

uint8_t player_image[] = {
	0b00111100, 0b00100100,
	0b00100100, 0b00111100,
	0b00011000, 0b01111110,
	0b00011000, 0b01100110,
};

uint8_t player_death_image[8] = {
	0b00100100, 0b00011000,
	0b00011000, 0b00100100, 
	0b00011000, 0b00111100,
	0b00011000, 0b00100100,
};

uint8_t treasure_image[] = {
	 0b00111100,	0b00100000, 
	 0b00111100,	0b00000100, 
	 0b00111100,	
};

uint8_t food_image[] = {
	0b11100000, 0b11100000,
};

uint8_t zombie_image[] = {
	0b10001000, 0b01010000,
	0b10101000, 0b01010000,
	0b10001000,


};

void setup_timer_1(){
   	TCCR1A = 0;
	TCCR1B = 5;
	TIMSK1 = 1;
	sei();
}

volatile uint32_t overflow_count = 0;
ISR(TIMER1_OVF_vect){
	overflow_count++;
}

void setup_timer_0(){
	TCCR0B |= 4;
	TIMSK0 = 1;
	//	(c) Turn on interrupts.
	sei();
}

static volatile double interval = 0;
ISR(TIMER0_OVF_vect){

	// Add timer overflow period to interval
	interval += TIMER_SCALE * 256.0 / FREQ;
	/*
	if ( interval >= 0.125 ) {
		interval = 0;
		PORTB ^= 1 << 2;
		PORTB ^= 1 << 3;

		// Equivalent:
		// uint8_t current_bit = BIT_VALUE(PORTD, 6);
		// WRITE_BIT(PORTD, 6, (1 - current_bit));
	}
	*/

}


void usb_serial_send(char * message) {
	// Cast to avoid "error: pointer targets in passing argument 1 
	//	of 'usb_serial_write' differ in signedness"
	usb_serial_write((uint8_t *) message, strlen(message));
}


void usb_serial_send_int(int value) {
	static char buffer[5];
	snprintf(buffer, sizeof(buffer), "%d", value);
	usb_serial_send( buffer );
}

void setup_draw_death(){
	for (int i = 0; i < 8; i++) {

        // Visit each row of output bitmap
        for (int j = 0; j < 8; j++) {
            // Kind of like: smiley_direct[i][j] = smiley_original[j][7-i].
            // Flip about the major diagonal.
            uint8_t bit_val = BIT_VALUE(player_death_image[j], (7 - i));
            WRITE_BIT(player_death_direct[i], j, bit_val);
        }
    }
}

void setup_pwm(){
	TC4H = OVERFLOW_TOP >> 8;
	OCR4C = OVERFLOW_TOP & 0xff;

	// (b)	Use OC4A for PWM. Remember to set DDR for C7.
	TCCR4A = BIT(COM4A1) | BIT(PWM4A);
	SET_BIT(DDRC, 7);

	// (c)	Set pre-scale to "no pre-scale" 
	TCCR4B = BIT(CS42) | BIT(CS41) | BIT(CS40);

	TCCR4D = 0;
}

void draw_double(uint8_t x, uint8_t y, double value, colour_t colour) {
	snprintf(buffer, sizeof(buffer), "%f", value);
	draw_string(x, y, buffer, colour);
}

void draw_int(uint8_t x, uint8_t y, int value, colour_t colour) {
	snprintf(buffer, sizeof(buffer), "%d", value);
	draw_string(x, y, buffer, colour);
}

void setup_zombies(){
	for(int i = 0; i < NUM_ZOMBIES; i++){
		Sprite zom;
		int x = i * (LCD_X - 4)/5;
		sprite_init(&zom,x,2,ZOMBIE_WIDTH,ZOMBIE_HEIGHT,zombie_image);
		zom.is_visible = 0;
		zom.dy = 0.2;
		zombies[i] = zom;
	}
}

int get_col(Sprite s){
    return (int) round(s.x) / COL_WIDTH;
}

int get_row(Sprite s){
    return (int) (round(s.y) + PLAT_HEIGHT -2)/ROW_HEIGHT - 1;
}

bool offset (Sprite s, int x, int y){
    int sx = x - round(s.x);
    int sy = y - round(s.y);
    if( 0 <= sx && sx < s.width && 0 <= sy && sy < s.height){
        int offset_s = sy * s.width + sx;
        if(s.bitmap[offset_s]!= ' '){
            return true;
        }

    }
    return false;
}

bool pixel_level_collision(Sprite s1, Sprite s2)
{
    for(int x = s2.x; x <= s2.x + s2.width; x++){
        for(int y = s2.y; y <= s2.y + s2.height;y++){
            if(offset(s1,x,y) && offset(s2,x,y)){
                return true;
            }
        }
    }
    return false;

}

void place_player_on_starting_block(){
	Sprite starting_blocks[40];
	int num_starts = 0;
	for(int i = 0; i < num_plats;i++){
		if(is_safe[i] && get_row(platform[i])==0){
			starting_blocks[num_starts] = platform[i];
			num_starts++;
		}
	}
	int start_index = num_starts > 1 ? rand()%num_starts : 0;
	player.x = starting_blocks[start_index].x + rand()%(starting_blocks[start_index].width-player.width);
	player.y = starting_blocks[start_index].y - player.width -1;
}

void setup_player(){
	sprite_init(&player,2,2,PLAYER_WIDTH,PLAYER_HEIGHT,player_image);
	player.is_visible = 0;
	player.dy = 0.4;
	place_player_on_starting_block();
}


void setup_platforms(){
	int h = LCD_Y;
	int w = LCD_X;
	int cols = w/COL_WIDTH;
	int rows = (h-2)/ROW_HEIGHT;
	int wanted = rows * cols * 90 / 100;
	int out_of = rows * cols;
	num_plats = 0;
	int safe_per_col[40] = {0};
	for(int row = 0; row < rows; row++){
		for(int col = 0; col < cols; col++){
			if (num_plats > 40) break;
			double p = (double)rand() / RAND_MAX;
			if ( p <= (double) wanted/out_of){
				wanted--;
				Sprite block;
				platform[num_plats]  = block;
				int x = col * (LCD_X-COL_WIDTH) / (cols-1) + rand()%(COL_WIDTH-PLAT_WIDTH);
				int y = 2 + (row+1) * ROW_HEIGHT - PLAT_HEIGHT;
				sprite_init(&block,x,y,PLAT_WIDTH,PLAT_HEIGHT,block_image);
				block.is_visible = 0;
				is_safe[num_plats] = true;
                safe_per_col[col]++;
				num_plats++;
			}
			out_of--;
		}
	}
	int num_forbidden = num_plats * 25 / 100;
	if (num_forbidden < 2) num_forbidden = 2;
	for (int i = 0; i < num_forbidden; i++){
		#define MAX_TRIALS (100)
		for(int trial = 0; trial < MAX_TRIALS;trial++){
			int plat_index = 1 + rand() % (num_plats-1);
			int col = get_col(platform[plat_index]);
			if(safe_per_col[col] > 1){
                is_safe[plat_index] = false; 
                safe_per_col[col]--;
                platform[plat_index].bitmap = forb_image;
                break;
            }
		}

	}
}

void draw_death_image(void) {
	if(lives==0){
		player.is_visible = 0;
		LCD_CMD(lcd_set_function, lcd_instr_basic | lcd_addr_horizontal);
		LCD_CMD(lcd_set_x_addr, (int)round(player.x));
		LCD_CMD(lcd_set_y_addr, (int)round(player.y / 8));

		for (int i = 0; i < 8; i++) {
			LCD_DATA(player_death_direct[i]);
		}
	}
}


void setup_treasure(){
	sprite_init(&treasure,0,LCD_Y-TREASURE_HEIGHT,TREASURE_WIDTH,TREASURE_HEIGHT,treasure_image);
	treasure.is_visible = 0;
	treasure.dx = 0.2;
	treasure.dy = 0;
}


void setup_game(void) {
	set_clock_speed(CPU_8MHz);
	lcd_init(LCD_DEFAULT_CONTRAST);
	adc_init();
	usb_init();
	clear_screen();
	CLEAR_BIT(DDRF,6); //SW2
    CLEAR_BIT(DDRF,5); //SW3
	SET_INPUT(DDRB,0); //Center Button
	SET_INPUT(DDRD,1); //Up Button 
	SET_INPUT(DDRB,1); //Left Button
	SET_INPUT(DDRB,7); //Down Button
	SET_INPUT(DDRD,0); //Right Button
    SET_BIT(DDRB,2);
    SET_BIT(DDRB,3);
	setup_pwm();
	setup_platforms();
	setup_player();
	setup_treasure();
	setup_zombies();
	setup_draw_death();
	draw_death_image();
	score = 0;
	lives = INIT_LIVES;
	start_time = (overflow_count * 65536.0 + TCNT1) * 1024.0/8000000.0;
	setup_timer_0();
	setup_timer_1();
	num_food_inventory = 0;
	num_zombies = NUM_ZOMBIES;
}

void setup_screen(){
	draw_string(10,5,"Quang Huy",FG_COLOUR);
	draw_string(10,15,"N10069275",FG_COLOUR);
	draw_string(10,25,"Press SW2",FG_COLOUR);
	draw_string(10,35,"to play",FG_COLOUR);
	show_screen();
}

int get_current_platform(Sprite s);
//Move Zombies


//Draw all
void draw(){
	for(int i = 0; i < num_plats; i++){
		sprite_draw(&platform[i]);
	}
	sprite_draw(&player);
	sprite_draw(&treasure);
	for(int i = 0; i < num_zombies; i++ ){
		sprite_draw(&zombies[i]);
	}
	for(int i = 0; i < num_food_inventory;i++){
		sprite_draw(&food[i]);
	}
}

void press_sw2() {
	if(BIT_IS_SET(PINF,6)){
		clear_screen();
		usb_serial_send("\nGame Start");
		usb_serial_send("\nPlayer position: ");
		usb_serial_send_int(player.x);
		usb_serial_send("");
		usb_serial_send_int(player.y);
		player.is_visible = 1;
		treasure.is_visible = 1;
		for(int i = 0; i < num_plats;i++){
			platform[i].is_visible = 1;
		}
	}
}

//game_pause
void game_pause(){
	bool static game_paused = false;
	double current_time = (overflow_count * 65536.0 + TCNT1) * 1024.0/8000000.0;
	int time = (int)round(current_time-start_time);
	if(BIT_IS_SET(PINB,0) && game_paused == false){
		game_paused = true;
		_delay_ms(33);

	}
	while(game_paused){
		clear_screen();
		treasure.is_visible = 0;
		player.is_visible = 0;
		for(int i = 0; i < num_plats;i++){
			platform[i].is_visible = 0;
		}
		draw_string(5,0,"Lives: ",FG_COLOUR);
		draw_int(50,0,lives,FG_COLOUR);
		draw_string(5,10,"Scores: ",FG_COLOUR);
		draw_int(50,10,score,FG_COLOUR);
		draw_string(5,20,"Time:",FG_COLOUR);
		draw_int(50,20, time/60, FG_COLOUR);
		if(time%60 < 10){
			draw_string(55,20,":0",FG_COLOUR);
			draw_int(65,20,time%60,FG_COLOUR);
		}
		else{
			draw_string(55,20,":",FG_COLOUR);
			draw_int(60,20,time%60,FG_COLOUR);
		}
		draw_string(5,30,"Food: ",FG_COLOUR);
		draw_int(50,30,num_food_inventory,FG_COLOUR);
		draw_string(5,40,"Zombies: ",FG_COLOUR);
		draw_int(50,40,num_zombies,FG_COLOUR);
		show_screen();
		usb_serial_send("\nGame paused");
		if( BIT_IS_SET(PINB,0) && game_paused == true){
			game_paused = false;
		}
	}
	if(game_paused==false){
		treasure.is_visible = 1;
		player.is_visible = 1;
		for(int i = 0; i < num_plats;i++){
			platform[i].is_visible = 1;
		}
	}
}

//Erase the death image 
void erase_death_image(void) {
    LCD_CMD(lcd_set_function, lcd_instr_basic | lcd_addr_horizontal);
    LCD_CMD(lcd_set_x_addr, (int)round(player.x));
    LCD_CMD(lcd_set_y_addr, (int)round(player.y) / 8);

    for (int i = 0; i < 8; i++) {
        LCD_DATA(0);
    }
}

static bool treasure_paused = false;
//Move treasure
void move_treasure(){
    if(BIT_IS_SET(PINF,5)==1){
        treasure_paused = ! treasure_paused;
    }
	if(usb_serial_available()){
		int c = usb_serial_getchar();
		if( c == 't'){
			treasure_paused = ! treasure_paused;
		}
	}

    if(!treasure_paused){
        treasure.x+=treasure.dx;
        int tl = (int)round(treasure.x);
        int tr = tl + treasure.width - 1;
        if(tl < 0 || tr > LCD_X){
            treasure.x -= treasure.dx;
            treasure.dx = -treasure.dx;
        }
    }
	if(pixel_level_collision(player,treasure)){
		treasure.y = -1000;
		lives+=2;
		setup_player();
	}

}

//Move platform 
void move_platform(){
	for(int i = 0; i < num_plats;i++){
		int left_adc = adc_read(0);
		if(get_row(platform[i])%2==0){
			platform[i].dx = (double) left_adc * 0.5 / 1024;
			platform[i].dy = 0;
			platform[i].x += platform[i].dx;
			int sl = (int)round(platform[i].x);
			int sr = sl + platform[i].width -1;
			if(sr >= LCD_X){
				platform[i].x = -5;
			}
		}
		else{
			platform[i].dx = -(double) left_adc * 0.5 / 1024;
			platform[i].dy = 0;
			platform[i].x += platform[i].dx;
			int sl = (int)round(platform[i].x);
			if(sl <= 0){
				platform[i].x = LCD_X + 5;
			}
		}
	}
}

void drop_food(){
	sprite_init(&food[num_food_inventory],player.x,player.y+player.height-2,3,2,food_image);
	int a = get_current_platform(player);
	food[num_food_inventory].dx = platform[a].dx;
	food[num_food_inventory].x += food[num_food_inventory].dx;
	num_food_inventory++;

}



//Play again
bool play_again(){
	clear_screen();
	player.is_visible = 0;
	treasure.is_visible = 0;
	for(int i = 0; i < num_plats;i++){
		platform[i].is_visible = 0;
	}
	draw_string(10,10,"Game Over!!!",FG_COLOUR);
	draw_string(10,20,"Replay: SW3",FG_COLOUR);
	draw_string(10,30,"Quit:   SW2",FG_COLOUR);
	show_screen();
    int key = BIT_IS_SET(PINF,5);
    return 1 == key;
}

//Get current platform
int get_current_platform(Sprite s){
    int sl = (int)round(s.x);
    int sr = sl + s.width;
    int sy = (int)round(s.y);
    for(int plat = 0; plat < num_plats;plat++){
        Sprite p = platform[plat];
        int pl = (int)round(p.x);
        int pr = pl + p.width;
        int py = (int)round(p.y);
        if(sr >= pl && sl <= pr && sy==py-s.height){
            return plat;
        }
    }
    return -1;
}


void process() {
	press_sw2();
	if(usb_serial_available()){
		int c = usb_serial_getchar();
		if(c == 'a'){
			player.x--;
		}
		else if(c == 'd'){
			player.x++;
		}
		else if(c == 'w'){
			player.dy = -1;
			player.y += player.dy;
		}
	}
	//Get the current platform of player to determine
	int plat = get_current_platform(player);
	bool static falling = false;
	bool died = false;
	if( BIT_IS_SET(PINB,7)){
		drop_food();
	}
	if(plat>=0){
		//Safe platform 
		if(is_safe[plat]==true){
			player.dx = platform[plat].dx;
			player.x += player.dx;
			if(BIT_IS_SET(PIND,1)){
				player.y-= 15;
			}
			if(falling==true){
				score++;
			}
			falling = false;
			if(BIT_IS_SET(PIND,0)==1){
				player.x++;
			}
			else if(BIT_IS_SET(PINB,1)==1){
				player.x--;
			}			
		}
		else{
			died = true;
			usb_serial_send("Collided with unsafe blocks");
		}

	}
	else{
		falling = true;
		player.y += player.dy;
	}
	//The player dies 
	int hl = (int)round(player.x);
    int hr = hl + player.width - 1;
    int ht = (int)round(player.y);
    int hb = ht + player.height - 1;

    if(hl < 0 || hr >= LCD_X || hb >= LCD_Y || ht < 0){
        died = true;
		usb_serial_send("\nOut of the screen");
    }
	move_treasure();
    if(died){
        falling = false;
		lives--;
        if(lives < 0){
            lives = 0;
        }
		usb_serial_send("\nPlayer die");
		usb_serial_send("\nScore: ");
		usb_serial_send_int(score);
		usb_serial_send("\nLives: ");
		usb_serial_send_int(lives);
        if(lives>0){
			//Use PWM to control backlight when player dies 
			TC4H = 10 >> 8;
			OCR4A = 10 & 0xff;
			_delay_ms(1500);
			TC4H = 600 >> 8;
			OCR4A = 600 & 0xff;
			_delay_ms(1500);			
			TC4H = 0 >> 8;
			OCR4A = 0 & 0xff;
            setup_player();
			usb_serial_send("\nPlayer respawns");
			usb_serial_send("\nPlayer position: ");
			usb_serial_send_int(player.x);
			usb_serial_send(" ");
			usb_serial_send_int(player.y);
        }
        else{
			draw_death_image(); //draw death image when lives = 0
			_delay_ms(250);
			erase_death_image();
			if(play_again()){  //Play Again 
				setup_game();
				setup_screen();
				start_game = false;
				game_over = false;

			}
			else if(BIT_IS_SET(PINF,6)){ //Quit game 
				game_over = true;
				clear_screen();
				draw_string(10,10,"N10069275",FG_COLOUR);
				show_screen();
			}
        }
    }
	move_platform();
	draw_death_image();
	draw();
	game_pause();
	show_screen();
	draw_death_image();
}

int main(void) {
	srand(start_time);
	setup_game();
	while(!start_game){
		setup_screen();
		if(BIT_IS_SET(PINF,6)==1){
			start_game = true;
		}
		if(usb_serial_available()){
			int cha = usb_serial_getchar();
			if ( cha == 's'){
				start_game = true;
			}
		}
	}
	while(!game_over) {
		process();
		_delay_ms(10);
		clear_screen();
	}
}