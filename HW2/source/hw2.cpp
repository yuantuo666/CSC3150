#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>

#define ROW 17
#define COLUMN 49
#define HORI_LINE '-'
#define VERT_LINE '|'
#define CORNER '+'
#define ADVENTURER '0'
#define WALL_CHAR '='
#define GOLD_CHAR '$'
#define EMPTY_CHAR ' '
#define NUM_WALLS 6
#define NUM_GOLD 6

// game status
#define RUNNING 0
#define WON 1
#define LOST -1
#define QUIT 2

// speed settings
#define WALL_MOVE_DELAY 100000 // 0.1 seconds
#define GOLD_MOVE_DELAY 200000 // 0.2 seconds
#define PRINT_DELAY 50000      // 0.05 seconds
#define INPUT_DELAY 10000      // 0.01 seconds

int player_x;
int player_y;
char map_grid[ROW][COLUMN + 1]; // +1 for null terminator in each row
int game_status_code = RUNNING;
int shards_remaining = NUM_GOLD;

pthread_mutex_t map_mutex; // mutex for map_grid

typedef struct
{
    int row;
    int col;
} Position;

typedef struct
{
    Position pos;  // Starting position (leftmost column)
    int direction; // 1: right, -1: left
} Wall;

typedef struct
{
    Position pos;
    int direction; // 1: right, -1: left
    int collected; // 0: not collected, 1: collected
} Gold;

Wall walls[NUM_WALLS];
Gold golds[NUM_GOLD];

/* functions sign */
int kbhit(void);
void map_print(void);
void init_map(void);
void init_walls(void);
void init_golds(void);
void *move_walls_thread(void *arg);
void *move_golds_thread(void *arg);
void *print_map_thread(void *arg);
void *input_thread_func(void *arg);
void update_game_status(void);
void clear_input_buffer(void);
void setup_terminal(void);
void reset_terminal(void);

struct termios raw_termios;

int kbhit(void)
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

void map_print(void)
{
    printf("\033[H\033[2J"); // clear
    pthread_mutex_lock(&map_mutex);
    for (int i = 0; i < ROW; i++)
        puts(map_grid[i]);
    // printf("Use W/A/S/D to move, Q to quit.\n");
    // printf("Gold Shards Remaining: %d\n", shards_remaining);
    pthread_mutex_unlock(&map_mutex);
}

void init_map(void)
{
    // init
    for (int i = 0; i < ROW; ++i)
        for (int j = 0; j < COLUMN; ++j)
            map_grid[i][j] = EMPTY_CHAR;

    // horizontal borders
    for (int j = 0; j < COLUMN; j++)
    {
        map_grid[0][j] = HORI_LINE;
        map_grid[ROW - 1][j] = HORI_LINE;
    }

    // vertical borders
    for (int i = 0; i < ROW; i++)
    {
        map_grid[i][0] = VERT_LINE;
        map_grid[i][COLUMN - 1] = VERT_LINE;
    }

    // corners
    map_grid[0][0] = CORNER;
    map_grid[0][COLUMN - 1] = CORNER;
    map_grid[ROW - 1][0] = CORNER;
    map_grid[ROW - 1][COLUMN - 1] = CORNER;

    // adventurer
    player_x = 8;
    player_y = 24;
    map_grid[player_x][player_y] = ADVENTURER;

    for (int i = 0; i < ROW; i++)
    {
        map_grid[i][COLUMN] = '\0';
    }
}

void init_walls(void)
{
    int wall_rows[NUM_WALLS] = {2, 4, 6, 10, 12, 14};
    srand(time(NULL));

    for (int i = 0; i < NUM_WALLS; ++i)
    {
        walls[i].pos.row = wall_rows[i];
        walls[i].direction = (i % 2 == 0) ? 1 : -1;

        walls[i].pos.col = rand() % (COLUMN - 15 - 2) + 1; // avoid borders

        // Place the wall on the map
        for (int j = 0; j < 15; ++j)
        {
            int current_col = walls[i].pos.col + j;
            if (current_col >= 1 && current_col < COLUMN - 1)
                map_grid[walls[i].pos.row][current_col] = WALL_CHAR;
        }
    }
}

void init_golds(void)
{
    int gold_rows[NUM_GOLD] = {1, 3, 5, 11, 13, 15};
    srand(time(NULL) + 1); // plus 1 make it differ from wall seed

    for (int i = 0; i < NUM_GOLD; ++i)
    {
        golds[i].pos.row = gold_rows[i];
        golds[i].direction = (rand() % 2) ? 1 : -1;
        golds[i].collected = 0;

        golds[i].pos.col = rand() % (COLUMN - 2) + 1; // avoid borders

        if (map_grid[golds[i].pos.row][golds[i].pos.col] == EMPTY_CHAR)
            map_grid[golds[i].pos.row][golds[i].pos.col] = GOLD_CHAR;
        else
        {
            // find the next available spot when occupied
            for (int j = 1; j < COLUMN - 1; ++j)
            {
                if (map_grid[golds[i].pos.row][j] == EMPTY_CHAR)
                {
                    golds[i].pos.col = j;
                    map_grid[golds[i].pos.row][j] = GOLD_CHAR;
                    break;
                }
            }
        }
    }
}

void *move_walls_thread(void *arg)
{
    Wall *wall = (Wall *)arg;
    while (game_status_code == RUNNING)
    {
        usleep(WALL_MOVE_DELAY);
        pthread_mutex_lock(&map_mutex);

        for (int j = 0; j < 15; ++j)
        {
            int current_col = wall->pos.col + j;
            if (current_col >= 1 && current_col < COLUMN - 1)
                map_grid[wall->pos.row][current_col] = EMPTY_CHAR;
            if (current_col < 1)
                map_grid[wall->pos.row][COLUMN - current_col - 2] = EMPTY_CHAR;
            if (current_col >= COLUMN - 1)
                map_grid[wall->pos.row][current_col - COLUMN + 2] = EMPTY_CHAR;
        }

        // update wall position
        wall->pos.col += wall->direction;
        if (wall->pos.col < 1)
            wall->pos.col = COLUMN - 2; // Reappear from right
        else if (wall->pos.col >= COLUMN - 1)
            wall->pos.col = 1; // Reappear from left

        // draw wall at new position
        for (int j = 0; j < 15; ++j)
        {
            int current_col = wall->pos.col + j;
            if (current_col >= 1 && current_col < COLUMN - 1)
                map_grid[wall->pos.row][current_col] = WALL_CHAR;
            if (current_col < 1)
                map_grid[wall->pos.row][COLUMN - current_col - 2] = WALL_CHAR; // Wrap around
            if (current_col >= COLUMN - 1)                                     // if this is at the border, then render to the beginning of the row
                map_grid[wall->pos.row][current_col - COLUMN + 2] = WALL_CHAR;
        }

        map_grid[wall->pos.row][COLUMN] = '\0';

        // check for collision with the adventurer
        if (wall->pos.row == player_x)
        {
            if (player_y >= wall->pos.col && player_y < wall->pos.col + 15)
            {
                game_status_code = LOST; // hit wall
            }
        }

        pthread_mutex_unlock(&map_mutex);
    }
    return NULL;
}

/* Function to handle moving gold shards */
void *move_golds_thread(void *arg)
{
    Gold *gold = (Gold *)arg;
    while (game_status_code == RUNNING && shards_remaining > 0 && !gold->collected)
    {
        usleep(GOLD_MOVE_DELAY);

        pthread_mutex_lock(&map_mutex);

        if (map_grid[gold->pos.row][gold->pos.col] == GOLD_CHAR)
            map_grid[gold->pos.row][gold->pos.col] = EMPTY_CHAR; // remove gold

        // update position
        gold->pos.col += gold->direction;
        if (gold->pos.col < 1)
            gold->pos.col = COLUMN - 2;
        else if (gold->pos.col > COLUMN - 2)
            gold->pos.col = 1;

        // check if adventurer collects the gold shard
        if (gold->pos.row == player_x && gold->pos.col == player_y && !gold->collected)
        {
            gold->collected = 1;
            shards_remaining--;
            if (shards_remaining == 0)
                game_status_code = WON;
        }

        // Place gold on the map if not collected
        if (!gold->collected)
        {
            if (map_grid[gold->pos.row][gold->pos.col] == EMPTY_CHAR)
                map_grid[gold->pos.row][gold->pos.col] = GOLD_CHAR;
        }

        map_grid[gold->pos.row][COLUMN] = '\0';

        pthread_mutex_unlock(&map_mutex);
    }
    return NULL;
}

void *print_map_thread(void *arg)
{
    while (game_status_code == RUNNING)
    {
        map_print();
        usleep(PRINT_DELAY); // Wait before next print
    }
    return NULL;
}

void *input_thread_func(void *arg)
{
    while (game_status_code == RUNNING)
    {
        if (kbhit()) // Check if a key is pressed
        {
            char ch = getchar();
            pthread_mutex_lock(&map_mutex);

            map_grid[player_x][player_y] = EMPTY_CHAR; // remove

            if (ch == 'w' || ch == 'W')
            {
                if (player_x > 1)
                    player_x--;
            }
            else if (ch == 's' || ch == 'S')
            {
                if (player_x < ROW - 2)
                    player_x++;
            }
            else if (ch == 'a' || ch == 'A')
            {
                if (player_y > 1)
                    player_y--;
            }
            else if (ch == 'd' || ch == 'D')
            {
                if (player_y < COLUMN - 2)
                    player_y++;
            }
            else if (ch == 'q' || ch == 'Q')
            {
                game_status_code = QUIT;
            }

            // check collision with walls
            for (int i = 0; i < NUM_WALLS; ++i)
            {
                if (player_x == walls[i].pos.row &&
                    player_y >= walls[i].pos.col &&
                    player_y < walls[i].pos.col + 15)
                {
                    game_status_code = LOST;
                }
            }

            // check if adventurer collects a gold shard
            for (int i = 0; i < NUM_GOLD; ++i)
            {
                if (!golds[i].collected &&
                    player_x == golds[i].pos.row &&
                    player_y == golds[i].pos.col)
                {
                    golds[i].collected = 1;
                    map_grid[golds[i].pos.row][golds[i].pos.col] = EMPTY_CHAR;
                    shards_remaining--;
                    if (shards_remaining == 0)
                        game_status_code = WON;
                }
            }

            // Place adventurer at new position
            map_grid[player_x][player_y] = ADVENTURER;

            pthread_mutex_unlock(&map_mutex);
        }
        usleep(INPUT_DELAY); // prevent high CPU usage
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    srand(time(NULL));

    // init
    init_map();
    init_walls();
    init_golds();
    pthread_mutex_init(&map_mutex, NULL);

    // setup terminal
    tcgetattr(STDIN_FILENO, &raw_termios);
    struct termios new_termios = raw_termios;

    new_termios.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    // threads
    pthread_t wall_threads[NUM_WALLS];
    pthread_t gold_threads_ids[NUM_GOLD];
    pthread_t printer_thread;
    pthread_t input_thread;

    pthread_create(&printer_thread, NULL, print_map_thread, NULL);
    pthread_create(&input_thread, NULL, input_thread_func, NULL);

    for (int i = 0; i < NUM_WALLS; ++i)
        pthread_create(&wall_threads[i], NULL, move_walls_thread, (void *)&walls[i]);
    for (int i = 0; i < NUM_GOLD; ++i)
        pthread_create(&gold_threads_ids[i], NULL, move_golds_thread, (void *)&golds[i]);

    // wait for threads to finish
    pthread_join(input_thread, NULL);
    pthread_join(printer_thread, NULL);

    for (int i = 0; i < NUM_WALLS; ++i)
        pthread_join(wall_threads[i], NULL);

    for (int i = 0; i < NUM_GOLD; ++i)
        pthread_join(gold_threads_ids[i], NULL);

    // reset terminal
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);

    // clear screen
    printf("\033[H\033[2J");

    if (game_status_code == WON)
        printf("You win the game!!\n");
    else if (game_status_code == LOST)
        printf("You lose the game!!\n");
    else if (game_status_code == QUIT)
        printf("You exit the game.\n");

    pthread_mutex_destroy(&map_mutex);
    return 0;
}
