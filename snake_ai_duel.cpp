/*
 * snake_ai_duel.cpp
 * Змейка 1 на 1 против непобедимого ИИ.
 * Зависимости: SDL2, SDL_ttf.
 * Компиляция (пример для g++):
 *   g++ snake_ai_duel.cpp -o snake_ai_duel -lSDL2 -lSDL_ttf
 */

#include <SDL.h>
#include <SDL_ttf.h>
#include <deque>
#include <vector>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>

// -------------------- Константы --------------------
const int SCREEN_WIDTH      = 1024;
const int SCREEN_HEIGHT     = 768;
const int CELL_SIZE         = 32;
const int GRID_WIDTH        = 25;
const int GRID_HEIGHT       = 18;
const int FIELD_OFFSET_X    = (SCREEN_WIDTH  - GRID_WIDTH  * CELL_SIZE) / 2;
const int FIELD_OFFSET_Y    = (SCREEN_HEIGHT - GRID_HEIGHT * CELL_SIZE) / 2 + 30;
const int UI_TOP_MARGIN     = 20;
const int FPS               = 60;
const float TICK_INTERVAL   = 0.15f;   // 150 мс на один игровой шаг
const float LERP_SPEED      = 20.0f;   // скорость интерполяции визуальной позиции

// Цветовая палитра (мультяшная, яркая)
const SDL_Color COLOR_BG        = { 34,  40,  49, 255 };
const SDL_Color COLOR_GRID      = { 57,  62,  70, 255 };
const SDL_Color COLOR_GRID_LINE = { 34,  40,  49, 255 };
const SDL_Color COLOR_PLAYER    = { 240, 84,  84, 255 };  // красный
const SDL_Color COLOR_PLAYER_H  = { 255, 107, 107, 255 };
const SDL_Color COLOR_AI        = { 78,  205, 196, 255 }; // бирюзовый
const SDL_Color COLOR_AI_H      = { 100, 230, 220, 255 };
const SDL_Color COLOR_FRUIT     = { 255, 180, 60,  255 };
const SDL_Color COLOR_SHADOW    = { 0,   0,   0,   80 };
const SDL_Color COLOR_UI_TEXT   = { 230, 230, 230, 255 };
const SDL_Color COLOR_BTN_BG    = { 255, 107, 107, 255 };
const SDL_Color COLOR_BTN_HOVER = { 255, 130, 130, 255 };

// -------------------- Вспомогательные структуры --------------------
struct Point {
    int x, y;
    Point() : x(0), y(0) {}
    Point(int x, int y) : x(x), y(y) {}
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return !(*this == o); }
};

enum Direction { UP, DOWN, LEFT, RIGHT, NONE };

// -------------------- Класс Змейки --------------------
class Snake {
public:
    std::deque<Point> body;
    Direction dir;
    Direction nextDir;   // буферизированное направление (для игрока)
    SDL_Color color;
    SDL_Color headColor;
    float visualX, visualY; // текущие визуальные координаты головы (в пикселях)
    float targetX, targetY; // целевые координаты после завершения шага
    bool growFlag;

    Snake(int startX, int startY, Direction d, SDL_Color c, SDL_Color hc)
        : dir(d), nextDir(d), color(c), headColor(hc), growFlag(false)
    {
        body.push_back(Point(startX, startY));
        // Добавляем ещё два сегмента позади
        Point back = startPosBehind(startX, startY, d);
        body.push_back(back);
        back = startPosBehind(back.x, back.y, d);
        body.push_back(back);

        visualX = targetX = startX * CELL_SIZE + FIELD_OFFSET_X + CELL_SIZE/2.0f;
        visualY = targetY = startY * CELL_SIZE + FIELD_OFFSET_Y + CELL_SIZE/2.0f;
    }

    void setDirection(Direction d) {
        // Защита от разворота на 180 градусов
        if ((d == UP    && dir != DOWN)  ||
            (d == DOWN  && dir != UP)    ||
            (d == LEFT  && dir != RIGHT) ||
            (d == RIGHT && dir != LEFT))
            nextDir = d;
    }

    void move() {
        dir = nextDir;
        Point head = body.front();
        Point newHead = head;
        switch (dir) {
            case UP:    newHead.y--; break;
            case DOWN:  newHead.y++; break;
            case LEFT:  newHead.x--; break;
            case RIGHT: newHead.x++; break;
            default: break;
        }
        body.push_front(newHead);
        if (!growFlag) {
            body.pop_back();
        } else {
            growFlag = false;
        }
        // Обновляем целевую позицию визуализации
        targetX = newHead.x * CELL_SIZE + FIELD_OFFSET_X + CELL_SIZE/2.0f;
        targetY = newHead.y * CELL_SIZE + FIELD_OFFSET_Y + CELL_SIZE/2.0f;
    }

    bool checkSelfCollision() const {
        const Point& head = body.front();
        for (size_t i = 1; i < body.size(); ++i) {
            if (head == body[i]) return true;
        }
        return false;
    }

    bool checkCollisionWith(const std::deque<Point>& otherBody) const {
        const Point& head = body.front();
        for (const auto& p : otherBody) {
            if (head == p) return true;
        }
        return false;
    }

    bool occupies(int x, int y) const {
        for (const auto& p : body) {
            if (p.x == x && p.y == y) return true;
        }
        return false;
    }

    void grow() {
        growFlag = true;
    }

    Point head() const { return body.front(); }

    void updateVisual(float dt) {
        // Плавное приближение визуальной позиции к целевой
        float dx = targetX - visualX;
        float dy = targetY - visualY;
        float step = LERP_SPEED * dt;
        if (fabs(dx) < step) visualX = targetX;
        else visualX += (dx > 0 ? step : -step);
        if (fabs(dy) < step) visualY = targetY;
        else visualY += (dy > 0 ? step : -step);
    }

private:
    Point startPosBehind(int x, int y, Direction d) {
        switch (d) {
            case UP:    return Point(x, y+1);
            case DOWN:  return Point(x, y-1);
            case LEFT:  return Point(x+1, y);
            case RIGHT: return Point(x-1, y);
            default:    return Point(x, y);
        }
    }
};

// -------------------- Класс Фрукта --------------------
class Fruit {
public:
    Point pos;
    SDL_Color color;
    float bobOffset; // для анимации "покачивания"
    Fruit(int x, int y, SDL_Color c = COLOR_FRUIT) : pos(x, y), color(c), bobOffset(0.0f) {}
};

// -------------------- ИИ противник (поиск безопасного пути) --------------------
class AIController {
public:
    // Вычисляет направление для змейки ИИ к фрукту с гарантией безопасности
    Direction getDirection(const Snake& ai, const Snake& player, const Fruit& fruit) {
        Point aiHead = ai.head();
        // Начальная позиция считается безопасной (мы на ней уже стоим)
        std::vector<Direction> safeDirs = getSafeDirections(aiHead, ai, player);

        if (safeDirs.empty()) {
            // Нет безопасных ходов – двигаемся в любом доступном (всё равно поражение)
            for (int d = UP; d <= RIGHT; ++d) {
                Direction dir = static_cast<Direction>(d);
                Point np = neighbour(aiHead, dir);
                if (inBounds(np) && !ai.occupies(np.x, np.y) && !player.occupies(np.x, np.y))
                    return dir;
            }
            return ai.dir; // если совсем тупик, продолжаем движение (умрём)
        }

        // Пытаемся найти путь к фрукту, который безопасен (после съедения фрукта будет выход)
        Direction best = safeDirs[0];
        int bestDist = 999999;
        for (Direction dir : safeDirs) {
            Point np = neighbour(aiHead, dir);
            // Симулируем шаг в этом направлении
            Snake simAi = ai;
            simAi.setDirection(dir);
            simAi.move();
            // Проверяем, безопасна ли позиция после шага (и после возможного съедения фрукта)
            if (isSafe(simAi, player, fruit)) {
                // Ищем путь от нового положения головы до фрукта
                int dist = bfsDistance(simAi.head(), fruit.pos, simAi, player);
                if (dist < bestDist) {
                    bestDist = dist;
                    best = dir;
                }
            }
        }
        return best;
    }

private:
    // Проверка, что после хода у змейки останется достаточно места для жизни
    bool isSafe(const Snake& ai, const Snake& player, const Fruit& fruit) {
        // Если съели фрукт – симулируем рост
        Snake simAi = ai;
        if (simAi.head() == fruit.pos) {
            simAi.grow();
        }
        // Выполним оценку доступного пространства от головы с помощью flood fill
        return countReachable(simAi.head(), simAi, player) > simAi.body.size();
    }

    int countReachable(Point start, const Snake& ai, const Snake& player) {
        std::vector<std::vector<bool>> visited(GRID_WIDTH, std::vector<bool>(GRID_HEIGHT, false));
        std::queue<Point> q;
        q.push(start);
        visited[start.x][start.y] = true;
        int cnt = 0;
        while (!q.empty()) {
            Point p = q.front(); q.pop();
            cnt++;
            for (int d = 0; d < 4; ++d) {
                Direction dir = static_cast<Direction>(d);
                Point np = neighbour(p, dir);
                if (inBounds(np) && !visited[np.x][np.y] &&
                    !ai.occupies(np.x, np.y) && !player.occupies(np.x, np.y)) {
                    visited[np.x][np.y] = true;
                    q.push(np);
                }
            }
        }
        return cnt;
    }

    // BFS расстояние от start до цели, игнорируя тела змей
    int bfsDistance(Point start, Point goal, const Snake& ai, const Snake& player) {
        if (start == goal) return 0;
        std::vector<std::vector<bool>> visited(GRID_WIDTH, std::vector<bool>(GRID_HEIGHT, false));
        std::queue<std::pair<Point, int>> q;
        q.push({start, 0});
        visited[start.x][start.y] = true;
        while (!q.empty()) {
            auto [p, dist] = q.front(); q.pop();
            for (int d = 0; d < 4; ++d) {
                Point np = neighbour(p, static_cast<Direction>(d));
                if (np == goal) return dist + 1;
                if (inBounds(np) && !visited[np.x][np.y] &&
                    !ai.occupies(np.x, np.y) && !player.occupies(np.x, np.y)) {
                    visited[np.x][np.y] = true;
                    q.push({np, dist+1});
                }
            }
        }
        return 999999; // недостижимо
    }

    std::vector<Direction> getSafeDirections(Point head, const Snake& ai, const Snake& player) {
        std::vector<Direction> dirs;
        for (int d = UP; d <= RIGHT; ++d) {
            Direction dir = static_cast<Direction>(d);
            Point np = neighbour(head, dir);
            if (inBounds(np) && !ai.occupies(np.x, np.y) && !player.occupies(np.x, np.y))
                dirs.push_back(dir);
        }
        return dirs;
    }

    Point neighbour(Point p, Direction d) {
        switch (d) {
            case UP:    return Point(p.x, p.y-1);
            case DOWN:  return Point(p.x, p.y+1);
            case LEFT:  return Point(p.x-1, p.y);
            case RIGHT: return Point(p.x+1, p.y);
            default:    return p;
        }
    }

    bool inBounds(Point p) {
        return p.x >= 0 && p.x < GRID_WIDTH && p.y >= 0 && p.y < GRID_HEIGHT;
    }
};

// -------------------- Основной класс игры --------------------
class Game {
public:
    Game() : window(nullptr), renderer(nullptr), font(nullptr),
             state(MENU), player(nullptr), ai(nullptr), fruit(nullptr),
             playerScore(0), aiScore(0), tickAccum(0.0f), quit(false),
             lastTickTime(0), gameOverTimer(0.0f), buttonHovered(false) {}

    ~Game() { cleanup(); }

    bool init() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
        window = SDL_CreateWindow("Snake AI Duel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window) return false;
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) return false;
        if (TTF_Init() == -1) return false;
        font = TTF_OpenFont("assets/arial.ttf", 24); // Замените на путь к шрифту
        if (!font) {
            // Попробуем системный шрифт
            font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);
        }
        if (!font) return false; // без шрифта не работаем
        srand(static_cast<unsigned int>(time(nullptr)));
        resetGame();
        return true;
    }

    void run() {
        SDL_Event e;
        Uint32 lastFrameTime = SDL_GetTicks();
        while (!quit) {
            Uint32 currentTime = SDL_GetTicks();
            float dt = (currentTime - lastFrameTime) / 1000.0f;
            lastFrameTime = currentTime;
            if (dt > 0.1f) dt = 0.1f; // защита от разрыва времени

            while (SDL_PollEvent(&e)) {
                handleEvent(e);
            }

            update(dt);
            render();
            SDL_Delay(1); // небольшая пауза, чтобы не нагружать ЦП
        }
    }

private:
    SDL_Window*   window;
    SDL_Renderer* renderer;
    TTF_Font*     font;
    enum GameState { MENU, PLAYING, GAMEOVER };
    GameState     state;
    Snake*        player;
    Snake*        ai;
    Fruit*        fruit;
    int           playerScore;
    int           aiScore;
    float         tickAccum;
    bool          quit;
    Uint32        lastTickTime;
    float         gameOverTimer;
    AIController  aiController;
    SDL_Rect      playButtonRect;
    bool          buttonHovered;

    // -------------------- Инициализация / сброс --------------------
    void resetGame() {
        delete player;
        delete ai;
        delete fruit;
        player = new Snake(GRID_WIDTH/4, GRID_HEIGHT/2, RIGHT, COLOR_PLAYER, COLOR_PLAYER_H);
        ai     = new Snake(3*GRID_WIDTH/4, GRID_HEIGHT/2, LEFT, COLOR_AI, COLOR_AI_H);
        spawnFruit();
        playerScore = 0;
        aiScore = 0;
        tickAccum = 0.0f;
        gameOverTimer = 0.0f;
        lastTickTime = SDL_GetTicks();
        playButtonRect = { SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 30, 200, 60 };
        buttonHovered = false;
    }

    void spawnFruit() {
        int x, y;
        do {
            x = rand() % GRID_WIDTH;
            y = rand() % GRID_HEIGHT;
        } while (player->occupies(x, y) || ai->occupies(x, y));
        delete fruit;
        fruit = new Fruit(x, y);
    }

    // -------------------- Обработка событий --------------------
    void handleEvent(SDL_Event& e) {
        if (e.type == SDL_QUIT) {
            quit = true;
        } else if (e.type == SDL_KEYDOWN) {
            if (state == PLAYING) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP:    player->setDirection(UP);    break;
                    case SDLK_DOWN:  player->setDirection(DOWN);  break;
                    case SDLK_LEFT:  player->setDirection(LEFT);  break;
                    case SDLK_RIGHT: player->setDirection(RIGHT); break;
                }
            } else if (state == GAMEOVER) {
                if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_SPACE) {
                    state = MENU;
                }
            }
        } else if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN) {
            int mx = e.motion.x, my = e.motion.y;
            if (state == MENU) {
                buttonHovered = (mx >= playButtonRect.x && mx <= playButtonRect.x + playButtonRect.w &&
                                 my >= playButtonRect.y && my <= playButtonRect.y + playButtonRect.h);
                if (e.type == SDL_MOUSEBUTTONDOWN && buttonHovered) {
                    state = PLAYING;
                    resetGame();
                }
            }
        }
    }

    // -------------------- Обновление логики --------------------
    void update(float dt) {
        if (state == PLAYING) {
            tickAccum += dt;
            if (tickAccum >= TICK_INTERVAL) {
                tickAccum -= TICK_INTERVAL;
                gameTick();
            }
            // Обновляем плавность визуального положения змеек
            player->updateVisual(dt);
            ai->updateVisual(dt);
        } else if (state == GAMEOVER) {
            gameOverTimer += dt;
            if (gameOverTimer > 2.0f) {
                // Автовозврат в меню через 2 сек
                state = MENU;
            }
        }
    }

    void gameTick() {
        // Ход ИИ
        Direction aiDir = aiController.getDirection(*ai, *player, *fruit);
        ai->setDirection(aiDir);
        ai->move();

        // Ход игрока
        player->move();

        // Проверка столкновений
        bool playerDead = false;
        bool aiDead = false;

        // Столкновение со стенами (выход за границы)
        Point ph = player->head();
        Point ah = ai->head();
        if (ph.x < 0 || ph.x >= GRID_WIDTH || ph.y < 0 || ph.y >= GRID_HEIGHT) playerDead = true;
        if (ah.x < 0 || ah.x >= GRID_WIDTH || ah.y < 0 || ah.y >= GRID_HEIGHT) aiDead = true;

        // Самопересечение
        if (player->checkSelfCollision()) playerDead = true;
        if (ai->checkSelfCollision()) aiDead = true;

        // Столкновение головой о тело противника
        if (player->checkCollisionWith(ai->body)) playerDead = true;
        if (ai->checkCollisionWith(player->body)) aiDead = true;

        // Столкновение головами (оба умирают)
        if (ph == ah) {
            playerDead = true;
            aiDead = true;
        }

        // Подбор фруктов
        bool fruitEaten = false;
        if (ph == fruit->pos) {
            player->grow();
            playerScore++;
            fruitEaten = true;
        }
        if (ah == fruit->pos) {
            ai->grow();
            aiScore++;
            fruitEaten = true;
        }
        if (fruitEaten) {
            spawnFruit();
        }

        if (playerDead || aiDead) {
            state = GAMEOVER;
            gameOverTimer = 0.0f;
            // Если умер только ИИ – победа игрока (маловероятно, но предусмотрим)
            // Если умер игрок – поражение
        }
    }

    // -------------------- Отрисовка --------------------
    void render() {
        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        SDL_RenderClear(renderer);

        drawField();

        if (state == MENU) {
            drawMenu();
        } else if (state == PLAYING || state == GAMEOVER) {
            drawSnake(*player, true);
            drawSnake(*ai, false);
            drawFruit();
            drawUI();

            if (state == GAMEOVER) {
                drawGameOverOverlay();
            }
        }

        SDL_RenderPresent(renderer);
    }

    void drawField() {
        // Фон клеток
        SDL_Rect gridRect = { FIELD_OFFSET_X, FIELD_OFFSET_Y, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE };
        SDL_SetRenderDrawColor(renderer, COLOR_GRID.r, COLOR_GRID.g, COLOR_GRID.b, COLOR_GRID.a);
        SDL_RenderFillRect(renderer, &gridRect);

        // Линии сетки (светлые)
        SDL_SetRenderDrawColor(renderer, COLOR_GRID_LINE.r, COLOR_GRID_LINE.g, COLOR_GRID_LINE.b, 100);
        for (int i = 0; i <= GRID_WIDTH; ++i) {
            int x = FIELD_OFFSET_X + i * CELL_SIZE;
            SDL_RenderDrawLine(renderer, x, FIELD_OFFSET_Y, x, FIELD_OFFSET_Y + GRID_HEIGHT * CELL_SIZE);
        }
        for (int j = 0; j <= GRID_HEIGHT; ++j) {
            int y = FIELD_OFFSET_Y + j * CELL_SIZE;
            SDL_RenderDrawLine(renderer, FIELD_OFFSET_X, y, FIELD_OFFSET_X + GRID_WIDTH * CELL_SIZE, y);
        }
    }

    void drawSnake(const Snake& snake, bool isPlayer) {
        // Тень
        for (size_t i = 0; i < snake.body.size(); ++i) {
            const Point& p = snake.body[i];
            SDL_Rect shadowRect = { FIELD_OFFSET_X + p.x * CELL_SIZE + 3,
                                    FIELD_OFFSET_Y + p.y * CELL_SIZE + 3,
                                    CELL_SIZE - 6, CELL_SIZE - 6 };
            SDL_SetRenderDrawColor(renderer, COLOR_SHADOW.r, COLOR_SHADOW.g, COLOR_SHADOW.b, COLOR_SHADOW.a);
            SDL_RenderFillRect(renderer, &shadowRect);
        }

        // Визуализация с плавными координатами для головы, остальные сегменты - клеточные
        // Голова (плавная)
        {
            SDL_Rect headRect;
            headRect.x = static_cast<int>(snake.visualX - CELL_SIZE/2.0f + 2);
            headRect.y = static_cast<int>(snake.visualY - CELL_SIZE/2.0f + 2);
            headRect.w = CELL_SIZE - 4;
            headRect.h = CELL_SIZE - 4;
            // Градиентная заливка (имитация через два цвета)
            SDL_SetRenderDrawColor(renderer, snake.headColor.r, snake.headColor.g, snake.headColor.b, 255);
            SDL_RenderFillRect(renderer, &headRect);
            // Внутренний блик
            SDL_Rect inner = { headRect.x + 4, headRect.y + 4, headRect.w - 8, headRect.h - 8 };
            SDL_SetRenderDrawColor(renderer, snake.color.r, snake.color.g, snake.color.b, 200);
            SDL_RenderFillRect(renderer, &inner);
            // Глаза (мультяшные)
            int eyeSize = CELL_SIZE / 6;
            // Левый глаз
            SDL_Rect eyeL = { headRect.x + headRect.w/4 - eyeSize/2, headRect.y + headRect.h/3 - eyeSize/2, eyeSize, eyeSize };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &eyeL);
            SDL_Rect pupilL = { eyeL.x + eyeSize/4, eyeL.y + eyeSize/4, eyeSize/2, eyeSize/2 };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &pupilL);
            // Правый глаз
            SDL_Rect eyeR = { headRect.x + 3*headRect.w/4 - eyeSize/2, headRect.y + headRect.h/3 - eyeSize/2, eyeSize, eyeSize };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &eyeR);
            SDL_Rect pupilR = { eyeR.x + eyeSize/4, eyeR.y + eyeSize/4, eyeSize/2, eyeSize/2 };
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &pupilR);
        }

        // Тело и хвост (клеточные позиции, без плавности для простоты)
        for (size_t i = 1; i < snake.body.size(); ++i) {
            const Point& p = snake.body[i];
            SDL_Rect segRect = { FIELD_OFFSET_X + p.x * CELL_SIZE + 2,
                                 FIELD_OFFSET_Y + p.y * CELL_SIZE + 2,
                                 CELL_SIZE - 4, CELL_SIZE - 4 };
            SDL_SetRenderDrawColor(renderer, snake.color.r, snake.color.g, snake.color.b, 255);
            SDL_RenderFillRect(renderer, &segRect);
            // Полоска по центру (мультяшный стиль)
            if (i % 2 == 0) {
                SDL_Rect stripe = { segRect.x + 4, segRect.y + segRect.h/2 - 2, segRect.w - 8, 4 };
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 80);
                SDL_RenderFillRect(renderer, &stripe);
            }
        }
    }

    void drawFruit() {
        // Анимация покачивания
        float time = SDL_GetTicks() / 500.0f;
        int offsetY = static_cast<int>(sin(time * 2.0f) * 3.0f);
        SDL_Rect fruitRect = { FIELD_OFFSET_X + fruit->pos.x * CELL_SIZE + 4,
                               FIELD_OFFSET_Y + fruit->pos.y * CELL_SIZE + 4 + offsetY,
                               CELL_SIZE - 8, CELL_SIZE - 8 };
        SDL_SetRenderDrawColor(renderer, COLOR_FRUIT.r, COLOR_FRUIT.g, COLOR_FRUIT.b, 255);
        SDL_RenderFillRect(renderer, &fruitRect);
        // Листик сверху
        SDL_Rect leafRect = { fruitRect.x + fruitRect.w/2 - 4, fruitRect.y - 5, 8, 10 };
        SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255);
        SDL_RenderFillRect(renderer, &leafRect);
        // Блик
        SDL_Rect shine = { fruitRect.x + 4, fruitRect.y + 4, 6, 6 };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 150);
        SDL_RenderFillRect(renderer, &shine);
    }

    void drawUI() {
        // Счёт игрока и ИИ
        std::string playerText = "Player: " + std::to_string(playerScore);
        std::string aiText = "AI: " + std::to_string(aiScore);
        SDL_Color white = { 255,255,255 };
        renderText(playerText, 50, UI_TOP_MARGIN, white);
        renderText(aiText, SCREEN_WIDTH - 150, UI_TOP_MARGIN, white);
    }

    void drawMenu() {
        // Заголовок
        renderText("SNAKE AI DUEL", SCREEN_WIDTH/2 - 150, 100, {255,255,255}, 36);

        // Кнопка "Играть!"
        SDL_SetRenderDrawColor(renderer, buttonHovered ? COLOR_BTN_HOVER.r : COLOR_BTN_BG.r,
                               buttonHovered ? COLOR_BTN_HOVER.g : COLOR_BTN_BG.g,
                               buttonHovered ? COLOR_BTN_HOVER.b : COLOR_BTN_BG.b, 255);
        SDL_RenderFillRect(renderer, &playButtonRect);
        // Обводка
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 100);
        SDL_RenderDrawRect(renderer, &playButtonRect);
        renderText("Play!", playButtonRect.x + 60, playButtonRect.y + 12, {255,255,255}, 24);
    }

    void drawGameOverOverlay() {
        // Затемнённый фон
        SDL_SetRenderDrawColor(renderer, 0,0,0, 180);
        SDL_Rect full = {0,0,SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(renderer, &full);
        std::string msg;
        if (player->checkSelfCollision() || player->head().x < 0 || player->head().x >= GRID_WIDTH ||
            player->head().y < 0 || player->head().y >= GRID_HEIGHT) {
            msg = "Game Over!";
        } else {
            msg = "AI Defeated!";
        }
        renderText(msg, SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 - 30, {255,100,100}, 32);
        renderText("Press Enter to return to menu", SCREEN_WIDTH/2 - 180, SCREEN_HEIGHT/2 + 20, COLOR_UI_TEXT, 18);
    }

    void renderText(const std::string& text, int x, int y, SDL_Color color, int fontSize = 24) {
        if (!font) return;
        TTF_Font* useFont = font;
        // Если нужен другой размер, можно загрузить динамически, но упростим
        SDL_Surface* surf = TTF_RenderText_Solid(useFont, text.c_str(), color);
        if (!surf) return;
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_Rect dest = { x, y, surf->w, surf->h };
        SDL_RenderCopy(renderer, texture, NULL, &dest);
        SDL_FreeSurface(surf);
        SDL_DestroyTexture(texture);
    }

    void cleanup() {
        delete player;
        delete ai;
        delete fruit;
        if (font) TTF_CloseFont(font);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
};

// -------------------- Точка входа --------------------
int main(int argc, char* argv[]) {
    Game game;
    if (!game.init()) {
        SDL_Log("Failed to initialize game: %s", SDL_GetError());
        return 1;
    }
    game.run();
    return 0;
}
