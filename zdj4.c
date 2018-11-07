#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cab202_graphics.h>
#include <cab202_sprites.h>
#include <cab202_timers.h>
#define MAX_BLOCKS (1000)
#define DELAY 10 /* milliseconds */
bool paused = false;
bool game_over = false;
bool update_screen = true;

sprite_id treasure;
sprite_id player;
sprite_id blocks[MAX_BLOCKS];
char * player_image = 
/**/	 " o "
/**/     "|-|"
/**/	 "|-|";
char * player_image_2 =
/**/     " o "    
/**/     "-_-"
/**/     "|-|";
char * player_image_3 = 
/**/     " o "
/**/     "@-@"
/**/     "|-|";
char * player_left_image  =
/**/     " o "
/**/     "/-/"
/**/     "/-/"; 
char * player_right_image = 
/**/     " o " 
/**/     "\\-\\"
/**/     "\\-\\";
char * player_jump_image = 
/**/     " o "
/**/     "^-^"
/**/     "|-|";
char * player_on_block = 
/**/     " o "
/**/     "<=>"
/**/     "|-|";
char * treasure_image_1= 
/**/	 "$$"
/**/     "$$";
char * treasure_image_2=
/**/     "@@"
/**/     "@@";
int lives = 10;
int score = 0;
char * s[10000];
timer_id  timer;
int minute = 0;
int second = 0;
double treasure_dx = 0.1;

//Setup the timer
void setup_timer(){
    timer = create_timer(1000);
}

//Process the timer
void process_timer(){
    if(timer_expired(timer)){
        second++;
        if(second==60){
            minute++;
            second = 0;
        }
    }
}

//Create the block with random width and character given
sprite_id create_block(int bx, int by, char c,int k,  int width){
    s[k] = (char *) malloc(width*2); 
    for(int i = 0; i < width * 2; i++){
        s[k][i] = c;
    }
    return sprite_create(bx,by,width,2,s[k]);
}

//Get random image for the player when respawning
char * GetPlayImage(){
    char * image_player;
    int num = rand()%10;
    if(num>7)
    {
        image_player = player_image;
    }
    else if(num >= 4)
    {
        image_player = player_image_2;
    }
    else{
        image_player = player_image_3;
    }
    return image_player;
}

//Return the random character for creating the block
char GetRandomChar(){
    char ch;
    int num = rand()%10;
    if(num>=6)
    {
        ch = '=';
    }
    else if(num >= 2)
    {
        ch =' ';
    }
    else{
        ch = 'x';
    }
    return ch;

}

//Get the key press by the user
int read_char()
{
    int key_code = paused ? wait_char() : get_char();

    if ('p' == key_code)
    {
        paused = !paused;
    }

    return key_code;
}

//Display the timer in appropriate format
void draw_timer(int x, int y){
    if(second < 10){
       draw_formatted(x,y,"Time: 0%d:0%d",minute,second);
   }
   else if(minute >= 10 && second < 10){
       draw_formatted(x,y,"Time: %d:0%d",minute,second);
   }
   else if(minute < 10 && second >= 10){
       draw_formatted(x,y,"Time: 0%d:%d",minute,second);
   }
   else{
       draw_formatted(x,y,"Time: 0%d:%d",minute,second);
   }
}

//Setup the display screen
void setup_screen_game(void) {
   draw_line(0,0,(int)screen_width()-1,0,'~');
   draw_formatted(2,2,"N10069275\t Lives: %d\t Score: %d",lives,score);
   draw_line(0,4,(int)screen_width()-1,4,'~');
   draw_timer(35,2);
}

//Setup the treasure
void setup_treasure(){
    int zx = 1 + rand() % (screen_width()-2) ;
	int zy = screen_height() - rand() % 2 - 2 ; 
	treasure = sprite_create(zx,zy,2,2,treasure_image_1);
	sprite_turn_to(treasure, 0.1, 0.0 );
}

//Setup the player
void setup_player(){
    player = sprite_create(20,6,3,3,player_on_block);
}

//Move the treasure with the key parameter
void move_treasure(int key){
        sprite_step(treasure);
        int x = round(sprite_x(treasure));
        double dy = sprite_dy(treasure);   
        //If the key collides with the bottom edge of the screen, it bounces back   
        if (x == screen_width() - 2 || x == 0) {   
            treasure_dx = -treasure_dx;
            sprite_set_image(treasure,treasure_image_2);
            if(x == 0){
                sprite_set_image(treasure,treasure_image_1);
            }
            if ( treasure_dx != sprite_dx(treasure) || dy != sprite_dy(treasure) ) {
			    sprite_back(treasure);
			    sprite_turn_to(treasure, treasure_dx, dy);
            }
        }
        //If key == 't' stop the treasure
        if (key == 't'){
            if(treasure->dx!=0){
                treasure->dx = 0;
            }
            else{
                treasure->dx = treasure_dx;
            }
        } 
}

//Setup the blocks
void setup_blocks(){
    int bx = 2;
    int by = rand()%2 + 9;
    int num_rows = (screen_height()-5)/6;
    int num_cols = screen_width()/10+1;
    for(int i = 1; i < num_rows;i++){
        for(int j = 1; j < num_cols;j++){
            char ch = GetRandomChar();
            int block_width = 5 + rand()%6;
            blocks[i*num_cols+j] = create_block(bx,by,ch,MAX_BLOCKS,block_width);
            bx+=(block_width+1);
        }
        by+=6;
        bx = 2;
    }
}

//Auto move the blocks to the left
void auto_block_left(sprite_id sid)
{
        sprite_turn_to(sid,-0.15,0.0);
        sprite_step(sid);
        if (round(sprite_x(sid)) == 0)
        {
            sid->x = screen_width() - 1;
            sprite_turn_to(sid,-0.15,0.0);
            sprite_step(sid);
        }
}

//Auto move the blocks to the right
void auto_block_right(sprite_id sid)
{
        sprite_turn_to(sid,0.1, 0.0);
        sprite_step(sid);

        if (round(sprite_x(sid)) == screen_width() - 1)
        {
            sid->x= 0;
            sprite_turn_to(sid,0.1,0.0);
            sprite_step(sid);
        }
}

//Auto move blocks all
void auto_block_all(sprite_id  sprites[],int num_rows,int num_cols){
    for (int i = 2; i < num_rows-1; i+=2)
    {
        for(int j = 1; j < num_cols;j++){
            auto_block_right(sprites[i*num_cols+j]);
        }
    }
    for(int i = 3; i < num_rows-1; i+=2){
        for(int j = 1; j < num_cols;j++){
            auto_block_left(sprites[i*num_cols+j]);
        }
    }

}

//Draw all sprites
void draw_sprites(sprite_id sids[],int num_rows,int num_cols)
{
    for (int i = 1; i < num_rows; i++)
    {
        for(int j = 1; j < num_cols;j++){
            sprite_draw(sids[i*num_cols+j]);
        }
    }
}

//Check whether if two sprites collide each other
bool sprites_collide(sprite_id s1, sprite_id s2)
{
    if ((!s1->is_visible) || (!s2->is_visible))
    {
        return false;
    }

    int l1 = round(sprite_x(s1));
    int l2 = round(sprite_x(s2));
    int t1 = round(sprite_y(s1));
    int t2 = round(sprite_y(s2));
    int r1 = l1 + sprite_width(s1) - 1;
    int r2 = l2 + sprite_width(s2) - 1;
    int b1 = t1 + sprite_height(s1) - 1;
    int b2 = t2 + sprite_height(s2) - 1;

    if (l1 > r2)
        return false;
    if (l2 > r1)
        return false;
    if (t1 > b2)
        return false;
    if (t2 > b1)
        return false;

    return true;
}

bool player_collision;

//Move player with the key given and check whether it collides with safe or forbidden blocks
void move_player(int key, int num_cols, int num_rows,sprite_id sprites[])
{
    int hx = round(sprite_x(player));
    int hy = round(sprite_y(player));
    sprite_turn_to(player,0.0,+0.1);
    if (key == 'a' && hx > 1)
    {
        player->bitmap = player_left_image;
        sprite_move(player,-1,0);
    }
    else if (key == 'd' && hx < (screen_width() - 1 - sprite_width(player)))
    {
        player->bitmap = player_right_image;
        sprite_move(player, +1, 0);
    }
    //Check whether the player collides with safe blocks
    for(int i = 1; i < num_rows;i++){
        for(int j = 1; j < num_cols;j++){
            if(sprites[i*num_cols+j]->bitmap[1]=='=' && 
            sprites_collide(sprites[i*num_cols+j],player) && sprites[i*num_cols+j]->x < player->x &&
            sprites[i*num_cols+j]->y < player->y+2){
                //Move together with safe blocks in their direction
                sprite_turn_to(player,sprites[i*num_cols+j]->dx,0.0);
                sprite_set_image(player,player_on_block);
                //Press 'w' to jump player to land on another block
                if (key == 'w' && hy > 6)
                {
                    sprite_set_image(player,player_jump_image);
                    sprite_move(player,3,-6);
                }
                //If the blocks go off the screen, player dies and respawns 
                if(sprites[i*num_cols+j]->x >= screen_width()-2 || 
                sprites[i*num_cols+j]->x <= 1){
                    lives = lives -1;
                    player->x = screen_width()/2;
                    player->y = 6;
                    sprite_set_image(player,GetPlayImage());
                }
            }
            //Blocks collide with forbidden blocks and respawn to the starting row 
            if(sprites[i*num_cols+j]->bitmap[1]=='x' && sprites_collide(sprites[i*num_cols+j],player)){
                    lives--;
                    sprite_set_image(player,GetPlayImage());
                    player->x = rand()%screen_width()/2+10;
                    player->y=6;
            }
        }
    }
    sprite_step(player);
}

//Check whether sprites collide with others 

void setup(){
    setup_timer();
    setup_screen();
    setup_blocks();
    setup_player();
    setup_treasure();
}

void checking_collision_treasure_bottom();

//Process the display screen, treasure, blocks and player in the game
void process_game(int num_rows, int num_cols, int key){
    move_treasure(key);
    auto_block_all(blocks,num_rows,num_cols);
    move_player(key,num_cols,num_rows,blocks);
    clear_screen();
    setup_screen_game();
    draw_sprites(blocks,num_rows,num_cols);
    sprite_draw(player);
    sprite_draw(treasure);
    checking_collision_treasure_bottom();
}

//Setup end game, display message, total score, and total time played
void end_game(int num_rows,int num_cols,int key)
{
    game_over = true;
    static char *msg_text = "GAME OVER! PRESS 'r' TO RESTART THE GAME OR PRESS 'q' TO QUIT THE GAME" ;
    int msg_width = strlen(msg_text);
    int msg_x = (screen_width() - msg_width) / 2;
    int msg_y = (screen_height() - 1) / 2;
    sprite_id msg = sprite_create(msg_x, msg_y, msg_width, 1, msg_text);
    clear_screen();
    sprite_draw(msg);
    draw_formatted(msg_x,msg_y+3,"Score:%d\t\t",score);
    draw_timer(msg_x+10,msg_y+3);
    show_screen();
    timer_pause(100);
    while(get_char() != 'q')
    {
        //Press 'r' to restart game
        if(get_char()=='r'){
            srand(48765);
            score = 0;
            lives = 10;
            minute = 0;
            second = 0;
            game_over = false;
            setup();
            process_timer();
            process_game(num_rows,num_cols,key);
            timer_pause(DELAY);
            show_screen();
            for(int i = 0; i< MAX_BLOCKS;i++){
                free(s[i]);
            }
             
        }   
    }
    wait_char();
}


//Checking whether the player collides with the treasure or fall the bottom of the screen
void checking_collision_treasure_bottom(){
    if(sprites_collide(player,treasure)){
        lives = lives + 2;
        sprite_hide(treasure);
        player->x =rand()%(screen_width()/2)+20;
        player->y=4+rand()%6;
    }
    if(sprite_y(player)>=screen_height()-1){
        lives = lives - 1;
        if(lives >0){
            sprite_set_image(player,GetPlayImage());
            player->x =rand()%(screen_width()/2)+20;
            player->y=4+rand()%6;
        }
    }
}


// Play one turn of game.
void process(void) {
    process_timer();
    int num_rows = (screen_height()-5)/6;
    int num_cols = screen_width()/10+1;
    int key = read_char();
    srand(486731);
    process_game(num_rows,num_cols,key);


}

// Clean up game
void cleanup(void) {
    // STATEMENTS
}

// Program entry point.
int main(void) {
    setup();
    int num_rows = (screen_height()-5)/6;
    int num_cols = screen_width()/10+1;
    int key = read_char();
    while ( !game_over ) {
        process();
        if (lives == 0){
            end_game(num_rows,num_cols,key);
        }

        if ( update_screen ) {
            show_screen();
        }

        timer_pause(DELAY);
    }

    cleanup();

    return 0;
}