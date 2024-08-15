#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <time.h>
#include <pthread.h>


#define MAX_WIDTH 30
#define MAX_HEIGHT 30
#define SHOOT_SPEED 50
#define MAX_ENEMIES 10

bool game_running = true;

typedef struct {
    bool isAlive;
    int enemyX;
    int enemyY;
    int blood_splatter[4][2];
} Enemy;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    char grid[MAX_HEIGHT][MAX_WIDTH];
    int width;
    int height;
    int playerX;
    int playerY;
    int playerDirection;
    int enemiesCount;
    Enemy enemies[MAX_ENEMIES];
} Map;

typedef struct {
    int life;
    int score;
} PlayerStatus;

typedef struct {
    Map *field;
    WINDOW *map_window;
    PlayerStatus *player_status;
    WINDOW *player_status_window;
} ThreadParams;

Map initializeGameField(const char *filename) {
    FILE *file = fopen(filename, "r");
    Map field;

    if (file == NULL) {
        printf("FILE OPEN ERROR!\n");
        exit(1);
    }

    fscanf(file, "%d %d", &field.width, &field.height);
    fgetc(file);

    if (field.width > MAX_WIDTH || field.height > MAX_HEIGHT) {
        printf("FIELD IS TO BIG");
        exit(1);
    }
    for (int i = 0; i < field.height; i++) {
        for (int j = 0; j < field.width; j++) {
            if (i == 0 || i == field.height - 1) {
                field.grid[i][j] = '#';
                continue;
            }
            if (j == 0 || j == field.width - 1) {
                field.grid[i][j] = '#';
                continue;
            }
            field.grid[i][j] = ' ';
        }
    }

    char line[100];
    int enemy_index = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        char type;
        int x, y;
        sscanf(line, "%c%d%d", &type, &x, &y);

        if (type == 'S') {
            field.playerX = x;
            field.playerY = y;
            field.playerDirection = 'N';
            field.grid[y][x] = '^';
        } else if (type == 'W') {
            field.grid[y][x] = '#';
        } else if (type == 'E') {
            field.enemies[enemy_index].enemyX = x;
            field.enemies[enemy_index].enemyY = y;
            field.enemies[enemy_index].isAlive = true;
            for (int blood_cord = 0; blood_cord < 4; ++blood_cord) {
                field.enemies[enemy_index].blood_splatter[blood_cord][0] = -1;
                field.enemies[enemy_index].blood_splatter[blood_cord][1] = -1;
            }
            field.grid[y][x] = 'E';
            enemy_index++;
        }
    }
    field.enemiesCount = enemy_index;

    fclose(file);
    return field;
}

void init_player_status(PlayerStatus *player_status) {
    player_status->life = 100;
    player_status->score = 0;
}

void print_player_status(PlayerStatus player_status, WINDOW *player_status_window) {

    wattron(player_status_window, COLOR_PAIR(1));
    box(player_status_window, '#', '#');

    wattroff(player_status_window, COLOR_PAIR(1));
    wattron(player_status_window, COLOR_PAIR(2));
    mvwprintw(player_status_window, 1, 5, "score: %d", player_status.score);
    wattroff(player_status_window, COLOR_PAIR(2));
    wattron(player_status_window, COLOR_PAIR(3));
    mvwprintw(player_status_window, 1, 18, "life: %d ", player_status.life);
    wattroff(player_status_window, COLOR_PAIR(3));

    wrefresh(player_status_window);
}


void generate_blood(Map *field, int x, int y, int enemy_index) {
    int bloodCount = 0;


    while (bloodCount < 4) {
        int offsetX = rand() % 3 - 1;
        int offsetY = rand() % 3 - 1;

        int bloodX = x + offsetX;
        int bloodY = y + offsetY;


        if (bloodX != x || bloodY != y) {
            if (bloodX >= 0 && bloodX < field->width && bloodY >= 0 && bloodY < field->height) {
                field->enemies[enemy_index].blood_splatter[bloodCount][0] = bloodX;
                field->enemies[enemy_index].blood_splatter[bloodCount][1] = bloodY;
                bloodCount++;
            }
        }
    }
}

void print_map(Map field, WINDOW *map_window) {

    wclear(map_window);
    for (int i = 0; i < field.enemiesCount; ++i) {
        if (field.enemies[i].isAlive == false) {
            if (field.playerX == field.enemies[i].enemyX && field.playerY == field.enemies[i].enemyY) {

            } else {
                bool isAliveEnemy = false;
                for (int j = 0; j < field.enemiesCount; ++j) {
                    if (i != j && field.enemies[j].isAlive) {
                        if (field.enemies[j].enemyX == field.enemies[i].enemyX &&
                            field.enemies[j].enemyY == field.enemies[i].enemyY) {
                            field.grid[field.enemies[i].enemyY][field.enemies[i].enemyX] = 'E';
                            isAliveEnemy = true;
                            break;
                        }
                    }
                }
                if (!isAliveEnemy) {
                    field.grid[field.enemies[i].enemyY][field.enemies[i].enemyX] = 'X';
                }
            }
        }
    }

    for (int i = 0; i < field.height; i++) {
        wmove(map_window, i, 0);
        for (int j = 0; j < field.width; j++) {
            bool in_blood = false;
            for (int enemy = 0; enemy < field.enemiesCount; ++enemy) {
                if (field.enemies[enemy].enemyY == i && field.enemies[enemy].enemyX == j &&
                    field.enemies[enemy].isAlive == false) {
                    in_blood = true;
                    break;
                }
            }

            for (int enemy = 0; enemy < field.enemiesCount; ++enemy) {
                for (int splatter = 0; splatter < 4; ++splatter) {
                    if (field.enemies[enemy].blood_splatter[splatter][0] == j &&
                        field.enemies[enemy].blood_splatter[splatter][1] == i) {
                        in_blood = true;
                        break;
                    }
                }
                if (in_blood) {
                    break;
                }
            }

            if (field.grid[i][j] == 'E') {
                wattron(map_window, in_blood ? COLOR_PAIR(6) : COLOR_PAIR(1));
                wprintw(map_window, "%c ", field.grid[i][j]);
                wattroff(map_window, in_blood ? COLOR_PAIR(6) : COLOR_PAIR(1));
            } else if (field.grid[i][j] == '^' || field.grid[i][j] == '>' || field.grid[i][j] == '<' ||
                       field.grid[i][j] == 'v') {

                wattron(map_window, in_blood ? COLOR_PAIR(5) : COLOR_PAIR(3));
                wprintw(map_window, "%c ", field.grid[i][j]);
                wattroff(map_window, in_blood ? COLOR_PAIR(5) : COLOR_PAIR(3));
            } else if (field.grid[i][j] == 'X') {
                wattron(map_window, COLOR_PAIR(4));
                wprintw(map_window, "%c ", field.grid[i][j]);
                wattroff(map_window, COLOR_PAIR(4));
            } else if (field.grid[i][j] == '#') {
                wattron(map_window, in_blood ? COLOR_PAIR(1) : COLOR_PAIR(0));
                wprintw(map_window, "%c ", field.grid[i][j]);
                wattroff(map_window, in_blood ? COLOR_PAIR(1) : COLOR_PAIR(0));
            } else {
                wattron(map_window, in_blood ? COLOR_PAIR(4) : COLOR_PAIR(0));
                wprintw(map_window, "%c ", field.grid[i][j]);
                wattroff(map_window, in_blood ? COLOR_PAIR(4) : COLOR_PAIR(0));
            }
        }
    }

    wrefresh(map_window);
}


void move_enemy(Map *field, const int enemy_index) {
    Point possibleMoves[] = {{0,  -1},
                             {0,  1},
                             {-1, 0},
                             {1,  0}};
    Point current_position = {field->enemies[enemy_index].enemyX, field->enemies[enemy_index].enemyY};
    int minDist = field->width + field->height;
    Point bestMove = {0, 0};

    for (int i = 0; i < 4; ++i) {
        Point newPos = {current_position.x + possibleMoves[i].x, current_position.y + possibleMoves[i].y};

        if ((newPos.x >= 0 && newPos.x < field->width) &&
            (newPos.y >= 0 && newPos.y < field->height) &&
            (field->grid[newPos.y][newPos.x] == ' ' || field->grid[newPos.y][newPos.x] == 'X')) {

            int dist = abs(newPos.x - field->playerX) + abs(newPos.y - field->playerY);

            if (dist < minDist) {
                minDist = dist;
                bestMove = newPos;
            }
        }
    }

    if (bestMove.x != current_position.x || bestMove.y != current_position.y) {
        field->grid[current_position.y][current_position.x] = ' ';
        field->enemies[enemy_index].enemyX = bestMove.x;
        field->enemies[enemy_index].enemyY = bestMove.y;
        field->grid[bestMove.y][bestMove.x] = 'E';
    }
}


void player_move(Map *field, const char newDirection) {
    switch (newDirection) {
        case 'N':
            field->playerDirection = 'N';
            field->grid[field->playerY][field->playerX] = '^';
            if (field->grid[field->playerY - 1][field->playerX] == ' ' ||
                field->grid[field->playerY - 1][field->playerX] == 'X') {
                field->grid[field->playerY][field->playerX] = ' ';
                field->playerY = field->playerY - 1;
                field->grid[field->playerY][field->playerX] = '^';
            }
            break;
        case 'S':
            field->playerDirection = 'S';
            field->grid[field->playerY][field->playerX] = 'v';
            if (field->grid[field->playerY + 1][field->playerX] == ' ' ||
                field->grid[field->playerY + 1][field->playerX] == 'X') {
                field->grid[field->playerY][field->playerX] = ' ';
                field->playerY = field->playerY + 1;
                field->grid[field->playerY][field->playerX] = 'v';
            }
            break;
        case 'E':
            field->playerDirection = 'E';
            field->grid[field->playerY][field->playerX] = '>';
            if (field->grid[field->playerY][field->playerX + 1] == ' ' ||
                field->grid[field->playerY][field->playerX + 1] == 'X') {
                field->grid[field->playerY][field->playerX] = ' ';
                field->playerX = field->playerX + 1;
                field->grid[field->playerY][field->playerX] = '>';
            }
            break;
        case 'W':
            field->playerDirection = 'W';
            field->grid[field->playerY][field->playerX] = '<';
            if (field->grid[field->playerY][field->playerX - 1] == ' ' ||
                field->grid[field->playerY][field->playerX - 1] == 'X') {
                field->grid[field->playerY][field->playerX] = ' ';
                field->playerX = field->playerX - 1;
                field->grid[field->playerY][field->playerX] = '<';
            }
            break;
        default:
            break;
    }
}


void bullet_delay(int milliseconds) {
    clock_t start_time = clock();
    clock_t end_time;

    while (1) {
        end_time = clock();
        if (((end_time - start_time) * 1000 / CLOCKS_PER_SEC) >= milliseconds) {
            break;
        }
    }
}

void *player_shoot(Map *field, PlayerStatus *playerStatus, WINDOW *map) {
    int bullet_x_cord;
    int bullet_y_cord;


    switch (field->playerDirection) {
        case 'N':
            bullet_x_cord = field->playerX;
            if (field->grid[field->playerY - 1][bullet_x_cord] == ' ' || field->grid[field->playerY - 1][bullet_x_cord] == 'X') {
                for (bullet_y_cord = field->playerY - 1;
                     field->grid[bullet_y_cord][bullet_x_cord] == ' ' || field->grid[bullet_y_cord][bullet_x_cord] == 'X'; --bullet_y_cord) {
                    field->grid[bullet_y_cord][bullet_x_cord] = '.';
                    print_map(*field, map);
                    wrefresh(map);
                    bullet_delay(SHOOT_SPEED);
                    field->grid[bullet_y_cord][bullet_x_cord] = ' ';
                }
                if (field->grid[bullet_y_cord][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == bullet_y_cord &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }

            } else if (field->grid[field->playerY - 1][bullet_x_cord] != ' ') {
                if (field->grid[field->playerY - 1][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == field->playerY - 1 &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }

                    playerStatus->score += 10;
                }
            }

            break;
        case 'S':
            bullet_x_cord = field->playerX;
            if (field->grid[field->playerY + 1][bullet_x_cord] == ' ' || field->grid[field->playerY + 1][bullet_x_cord] == 'X') {
                for (bullet_y_cord = field->playerY + 1;
                     field->grid[bullet_y_cord][bullet_x_cord] == ' ' || field->grid[bullet_y_cord][bullet_x_cord] == 'X'; ++bullet_y_cord) {
                    field->grid[bullet_y_cord][bullet_x_cord] = '.';
                    print_map(*field, map);
                    wrefresh(map);
                    bullet_delay(SHOOT_SPEED);
                    field->grid[bullet_y_cord][bullet_x_cord] = ' ';
                }
                if (field->grid[bullet_y_cord][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == bullet_y_cord &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }
            } else if (field->grid[field->playerY + 1][bullet_x_cord] != ' ') {
                if (field->grid[field->playerY + 1][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == field->playerY + 1 &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }
            }
            break;
        case 'E':
            bullet_y_cord = field->playerY;
            if (field->grid[bullet_y_cord][field->playerX + 1] == ' ' || field->grid[bullet_y_cord][field->playerX + 1] == 'X') {
                for (bullet_x_cord = field->playerX + 1;
                     field->grid[bullet_y_cord][bullet_x_cord] == ' ' || field->grid[bullet_y_cord][bullet_x_cord] == 'X'; ++bullet_x_cord) {
                    field->grid[bullet_y_cord][bullet_x_cord] = '.';
                    print_map(*field, map);
                    wrefresh(map);
                    bullet_delay(SHOOT_SPEED);
                    field->grid[bullet_y_cord][bullet_x_cord] = ' ';
                }
                if (field->grid[bullet_y_cord][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == bullet_y_cord &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }
            } else if (field->grid[bullet_y_cord][field->playerX + 1] != ' ') {
                if (field->grid[bullet_y_cord][field->playerX + 1] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == bullet_y_cord &&
                            field->enemies[enemy].enemyX == field->playerX + 1) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }
            }
            break;
        case 'W':
            bullet_y_cord = field->playerY;
            if (field->grid[bullet_y_cord][field->playerX - 1] == ' ' || field->grid[bullet_y_cord][field->playerX - 1] == 'X' ) {
                for (bullet_x_cord = field->playerX - 1;
                     field->grid[bullet_y_cord][bullet_x_cord] == ' ' || field->grid[bullet_y_cord][bullet_x_cord] == 'X'; --bullet_x_cord) {
                    field->grid[bullet_y_cord][bullet_x_cord] = '.';
                    print_map(*field, map);
                    wrefresh(map);
                    bullet_delay(SHOOT_SPEED);
                    field->grid[bullet_y_cord][bullet_x_cord] = ' ';
                }
                if (field->grid[bullet_y_cord][bullet_x_cord] == 'E') {
                    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                        if (field->enemies[enemy].enemyY == bullet_y_cord &&
                            field->enemies[enemy].enemyX == bullet_x_cord) {
                            field->enemies[enemy].isAlive = false;
                            generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                            field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        }
                    }
                    playerStatus->score += 10;
                }
            } else if (field->grid[bullet_y_cord][field->playerX - 1] == 'E') {
                for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
                    if (field->enemies[enemy].enemyY == bullet_y_cord &&
                        field->enemies[enemy].enemyX == field->playerX - 1) {
                        field->enemies[enemy].isAlive = false;
                        generate_blood(field, field->enemies[enemy].enemyX, field->enemies[enemy].enemyY, enemy);
                        field->grid[field->enemies[enemy].enemyY][field->enemies[enemy].enemyX] = 'X';
                        playerStatus->score += 10;
                    }
                }
            }
            break;
        default:
            break;
    }
    return NULL;
}

void drawHotlineArt() {


    attron(COLOR_PAIR(1));


    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    (void)maxY;
    const char *hotlineArt[] = {
            "     )    )        (    (       )     ",
            "  ( /( ( /(   *   ))\\ ) )\\ ) ( /(     ",
            "  )\\()))\\())` )  /(()/((()/( )\\())(   ",
            "  ((_)\\((_)\\  ( )(_))(_))/(_)|(_)\\ )\\ ",
            "  _((_) ((_)(_(_()|_)) (_))  _((_|(_) ",
            " | || |/ _ \\|_   _| |  |_ _|| \\| | __|",
            " | __ | (_) | | | | |__ | | | .` | _| ",
            " |_||_|\\___/  |_| |____|___||_|\\_|___|",
    };

    int startY = 1;


    for (int i = 0; i < 8; i++) {
        move(startY + i, (maxX - 38) / 2);
        attron(A_BOLD);
        printw("%s\n", hotlineArt[i]);
    }
    char press_key[] = "(PRESS ANY KEY TO CONTINUE)";
    move(startY + 9, (maxX - strlen(press_key)) / 2);
    printw("%s", press_key);
    attroff(COLOR_PAIR(1));
    refresh();
    getchar();
    clear();
    refresh();
}


void damage_check(Map *field, PlayerStatus *player_status, WINDOW *player_status_window) {
    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
        if (field->enemies[enemy].isAlive) {
            int enemyX = field->enemies[enemy].enemyX;
            int enemyY = field->enemies[enemy].enemyY;

            if (abs(enemyX - field->playerX) <= 1 && abs(enemyY - field->playerY) <= 1) {
                player_status->life -= 15;
                print_player_status(*player_status, player_status_window);
                wrefresh(player_status_window);
            }
        }
    }
}

void *move_enemies_periodically(void *arg) {
    ThreadParams *params = (ThreadParams *) arg;
    Map *field = params->field;
    WINDOW *map_window = params->map_window;
    PlayerStatus *player_status = params->player_status;
    WINDOW *player_status_window = params->player_status_window;
    while (game_running) {


        for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
            if (field->enemies[enemy].isAlive) {
                move_enemy(field, enemy);

            }
        }

        print_map(*field, map_window);
        wrefresh(map_window);
        damage_check(field, player_status, player_status_window);
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 500000000;

        nanosleep(&ts, NULL);
    }
 return NULL;
}

void reset_level(Map *field, PlayerStatus *player_status,const char *level) {
    *field = initializeGameField(level);
    init_player_status(player_status);
}

void init_new_level(Map *field,const char *level){
    *field = initializeGameField(level);
}

void game_over_art() {
    clear();
    refresh();
    attron(COLOR_PAIR(1));


    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    (void)maxY;
    const char *game_over_art[] = {
           " __ __  _____  _     _ _______  ______  _______",
           "  \\_/  |     | |     | |_____| |_____/ |______",
           "   |   |_____| |_____| |     | |    \\_ |______",
           " ______  _______ _______ ______",
           "|      \\ |______ |_____| |     \\  ",
           "|_____/  |______ |     | |_____/  ",
     };

    int startY = 0;

    for (int i = 0; i < 6; i++) {
        move(startY + i, (maxX - 38) / 2);
        attron(A_BOLD);
        printw("%s", game_over_art[i]);
    }
    char press_key[] = " Press 'R' to restart or 'Q' to quit.";
    move(startY + 9, (maxX - strlen(press_key)) / 2);
    printw("%s", press_key);
    attroff(COLOR_PAIR(1));
    refresh();
}

void next_level_art() {
    clear();
    refresh();
    attron(COLOR_PAIR(1));


    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    (void)maxY;
    const char *next_level_art[] = {
            "______   _____   _____  ______       _____  _____  ______",
            "|  ____ |     | |     | |     \\        |   |     | |_____]",
            "|_____| |_____| |_____| |_____/      __|   |_____| |_____]",


    };

    int startY = 0;

    for (int i = 0; i < 3; i++) {
        move(startY + i, (maxX - 47) / 2);
        attron(A_BOLD);
        printw("%s", next_level_art[i]);
    }

    char press_key[] = "Press any key to continue.";
    move(startY + 5, (maxX - strlen(press_key)) / 2);
    printw("%s", press_key);
    attroff(COLOR_PAIR(1));
    refresh();
}

void print_win_art(){
    clear();
    refresh();
    attron(COLOR_PAIR(1));

    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    (void)maxY;
    const char *next_level_art[] = {
            "+--^----------,--------,-----,--------^-,",
            "| |||||||||   `--------'     |          O",
            "`+---------------------------^----------|",
            " `\\_,---------,---------,--------------'",
            "    / XXXXXX /'|       /'",
            "   / XXXXXX /  `\\    /'",
            "  / XXXXXX /`-------'",
            " / XXXXXX /",
            "/ XXXXXX /",
            "(________(",
            "`------'",
    };

    int startY = 1;

    for (int i = 0; i < 10; i++) {
        move(startY + i, (maxX - 47) / 2);
        attron(A_BOLD);
        printw("%s", next_level_art[i]);
    }

    char press_key[] = "Victory! Enemies annihilated, arena cleared. You are invincible.";
    move(startY + 11, (maxX - strlen(press_key)) / 2); // Placed the message right below the art
    printw("%s", press_key);
    attroff(COLOR_PAIR(1));
    refresh();
}

bool level_clear_check(Map *field){
    bool alive_count = 0;
    for (int enemy = 0; enemy < field->enemiesCount; ++enemy) {
        if(field->enemies[enemy].isAlive){
            alive_count += 1;
        }
    }
    return alive_count > 0 ? false : true;
}

void game(Map *field, WINDOW *map_window, PlayerStatus *player_status, WINDOW *player_status_window) {
    nodelay(stdscr, TRUE);
    int ch;
    while (player_status->life > 0 && level_clear_check(field) == false) {
        ch = getch();
        if (ch == 'q') {
            break;
        }
        switch (ch) {
            case KEY_UP:
                player_move(field, 'N');
                print_map(*field, map_window);
                break;
            case KEY_DOWN:
                player_move(field, 'S');
                print_map(*field, map_window);
                break;
            case KEY_LEFT:
                player_move(field, 'W');
                print_map(*field, map_window);
                break;
            case KEY_RIGHT:
                player_move(field, 'E');
                print_map(*field, map_window);
                break;
            case ' ':
                player_shoot(field, player_status, map_window);
                print_map(*field, map_window);
                print_player_status(*player_status, player_status_window);
                break;
            case ERR:
                continue;

            default:
                break;
        }
        refresh();
    }
    nodelay(stdscr, FALSE);
}



int main() {
    const char *levels[] = {"level1.txt", "level2.txt", "level3.txt"};
    int current_level = 0;
    Map field = initializeGameField(levels[0]);
    bool game_over = false;
    PlayerStatus player_status = {0};
    init_player_status(&player_status);
    srand(time(NULL));

    WINDOW *player_status_window;
    WINDOW *map_window;
    //curses init
    initscr();
    player_status_window = newwin(3, 30, LINES - field.height, (COLS - 30) / 2);
    map_window = newwin(field.height, field.width * 2, 2, (COLS - field.width * 2) / 2);
    raw();
    noecho();
    keypad(stdscr, true);
    start_color();
    //colors
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_RED);
    init_pair(5, COLOR_GREEN, COLOR_RED);
    init_pair(6, COLOR_WHITE, COLOR_RED);
    drawHotlineArt();
    print_map(field, map_window);
    print_player_status(player_status, player_status_window);
    ThreadParams threadParams = {&field, map_window, &player_status, player_status_window};
    pthread_t enemy_thread;
    pthread_create(&enemy_thread, NULL, move_enemies_periodically, &threadParams);

    while (!game_over) {
        game(&field, map_window, &player_status, player_status_window);
        if (player_status.life <= 0) {
            wclear(map_window);
            wrefresh(map_window);
            wclear(player_status_window);
            wrefresh(player_status_window);
            game_running = false;
            game_over_art();
            refresh();

            int restart_choice;
            while ((restart_choice = getch()) != 'q') {
                if (restart_choice == 'r') {
                    clear();
                    refresh();
                    current_level = 0;
                    reset_level(&field, &player_status, levels[0]);
                    print_map(field, map_window);
                    print_player_status(player_status, player_status_window);
                    game_running = true;
                    game_over = false;
                    pthread_create(&enemy_thread, NULL, move_enemies_periodically, &threadParams);
                    game(&field, map_window, &player_status, player_status_window);
                    break;
                }
            }
            if (restart_choice == 'q') {
                game_over = true;
            }
        }
        if(level_clear_check(&field)){
            current_level++;
            if(current_level <= 2) {
                getch();
                wclear(map_window);
                wrefresh(map_window);
                wclear(player_status_window);
                wrefresh(player_status_window);
                game_running = false;
                next_level_art();
                refresh();
                getch();
                clear();
                refresh();
                game_running = true;
                pthread_create(&enemy_thread, NULL, move_enemies_periodically, &threadParams);
                init_new_level(&field, levels[current_level]);
                print_player_status(player_status, player_status_window);
                print_map(field, map_window);
                game(&field, map_window, &player_status, player_status_window);
                refresh();
            }
            else{
                getch();
                wclear(map_window);
                wrefresh(map_window);
                wclear(player_status_window);
                wrefresh(player_status_window);
                game_running = false;
                print_win_art();
                refresh();
                getch();
                game_over = true;
            }
        }
    }
    pthread_cancel(enemy_thread);
    pthread_join(enemy_thread, NULL);
    endwin();
    return 0;
}
