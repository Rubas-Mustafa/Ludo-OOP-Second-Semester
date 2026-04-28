
//  Group: 25K-0921 Rubas Mustafa | 25K-0544 Syeda Nisa Batool
// to run: g++ main.cpp -o app -std=c++17 -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
// and then ./app
// on mac
#define RAYLIB_NO_MACRO_COLORS
#include "raylib.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include <cstdio>
#include <cmath>

//  CONSTANTS

const int SCREEN_W  = 860;
const int SCREEN_H  = 800;
const int CELL_SIZE = 50;
const int BOARD_OFF = 25;
const int PATH_LEN  = 52;

const int SAFE_SQUARES[]   = {0, 8, 13, 22, 27, 35, 40, 48};
const int SAFE_SQUARES_LEN = 8;

bool isSafeSquare(int idx)
{
    for (int i = 0; i < SAFE_SQUARES_LEN; i++)
        if (SAFE_SQUARES[i] == idx) return true;
    return false;
}

const int PATH_COL[52] = {
    6, 6, 6, 6, 6,
    5, 4, 3, 2, 1, 0,
    0, 0,
    1, 2, 3, 4, 5, 6,
    6, 6, 6, 6, 6,
    6, 8,
    8, 8, 8, 8, 8,
    9,10,11,12,13,14,
    14,14,
    13,12,11,10, 9,
    8, 8, 8, 8, 8,
    8, 7, 6
};
const int PATH_ROW[52] = {
    13,12,11,10, 9,
    8, 8, 8, 8, 8, 8,
    7, 6,
    6, 6, 6, 6, 6, 6,
    5, 4, 3, 2, 1,
    0, 0,
    1, 2, 3, 4, 5,
    6, 6, 6, 6, 6, 6,
    7, 8,
    8, 8, 8, 8, 8,
    9,10,11,12,13,
    14,14,14
};

const int RED_HOME_C[6]  = {7,7,7,7,7,7};
const int RED_HOME_R[6]  = {13,12,11,10,9,8};
const int BLUE_HOME_C[6] = {1,2,3,4,5,6};
const int BLUE_HOME_R[6] = {7,7,7,7,7,7};

const int RED_HOME_ENTRY  = 51;
const int BLUE_HOME_ENTRY = 12;

Vector2 CellCentre(int col, int row)
{
    return {
        (float)(BOARD_OFF + col * CELL_SIZE + CELL_SIZE / 2),
        (float)(BOARD_OFF + row * CELL_SIZE + CELL_SIZE / 2)
    };
}

class SoundSystem
{
    static const int SR = 44100;  
    Sound sndRoll;
    Sound sndMove;
    Sound sndKill;
    bool  ready = false;

    static Sound buildSound(short *buf, int samples)
    {
        Wave w;
        w.frameCount = (unsigned int)samples;
        w.sampleRate = SR;
        w.sampleSize = 16;
        w.channels   = 1;
        w.data       = buf;
        return LoadSoundFromWave(w);
    }

    static inline short clampSample(float v)
    {
        if (v >  1.f) v =  1.f;
        if (v < -1.f) v = -1.f;
        return (short)(v * 30000.f);
    }

    static inline float noise(int i)
    {
        unsigned int x = (unsigned int)i;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return ((float)(x & 0xFFFF) / 32768.f) - 1.f;
    }

    static Sound makeDiceRoll()
    {
        const int N = (int)(SR * 0.42f);
        short *buf = new short[N]();

        const int clicks[4] = { 0, (int)(SR*0.10f), (int)(SR*0.21f), (int)(SR*0.33f) };

        for (int c = 0; c < 4; c++)
        {
            int start = clicks[c];
            int len = (int)(SR * 0.06f);
            for (int i = 0; i < len && (start+i) < N; i++)
            {
                float t   = (float)i / len;
                float env = expf(-12.f * t);              // fast decay
                float n   = noise(start + i * 7 + c * 1337);
                // Mix noise with a short 180Hz percussive tone
                float tone = sinf(2.f * 3.14159f * 180.f * t) * 0.4f;
                buf[start + i] += clampSample(env * (n * 0.7f + tone) * 0.55f);
            }
        }

        for (int i = 0; i < N; i++)
        {
            float t   = (float)i / N;
            float env = expf(-4.f * t);
            float rumble = sinf(2.f * 3.14159f * 80.f * t) * 0.18f;
            // Add — won't overflow much since it's quiet
            int val = buf[i] + clampSample(env * rumble);
            buf[i]  = (short)(val > 30000 ? 30000 : (val < -30000 ? -30000 : val));
        }

        Sound s = buildSound(buf, N);
        delete[] buf;
        return s;
    }

    static Sound makeTokenMove()
    {
        const int N = (int)(SR * 0.14f);
        short *buf = new short[N];

        for (int i = 0; i < N; i++)
        {
            float t = (float)i / N;

            float impact = 0.f;
            if (i < (int)(SR * 0.005f))
            {
                float te = (float)i / (SR * 0.005f);
                impact = noise(i * 3) * expf(-20.f * te) * 1.0f;
            }

            float env  = expf(-22.f * t);
            float tone = sinf(2.f * 3.14159f * 700.f * t) * env * 0.75f;

            float harm = sinf(2.f * 3.14159f * 1400.f * t) * env * 0.15f;

            buf[i] = clampSample(impact + tone + harm);
        }

        Sound s = buildSound(buf, N);
        delete[] buf;
        return s;
    }

    static Sound makeTokenKill()
    {
        const int N = (int)(SR * 0.55f);
        short *buf  = new short[N];

        for (int i = 0; i < N; i++)
        {
            float t = (float)i / N;

            float freq = 80.f + 440.f * expf(-4.5f * t);

            float phase = 2.f * 3.14159f * (80.f * t + (440.f / 4.5f) * (1.f - expf(-4.5f * t)));
            float wave  = sinf(phase);

            float tremolo = 0.75f + 0.25f * sinf(2.f * 3.14159f * 8.f * t);

            float env = (1.f - expf(-30.f * t)) * expf(-2.8f * t);

            float n = noise(i * 5) * 0.08f;

            buf[i] = clampSample(env * tremolo * (wave * 0.85f + n));
        }

        Sound s = buildSound(buf, N);
        delete[] buf;
        return s;
    }

public:
    void init()
    {
        InitAudioDevice();
        if (!IsAudioDeviceReady()) { ready = false; return; }
        sndRoll = makeDiceRoll();
        sndMove = makeTokenMove();
        sndKill = makeTokenKill();
        ready   = true;
    }

    void playRoll() { if (ready) PlaySound(sndRoll); }
    void playMove() { if (ready) PlaySound(sndMove); }
    void playKill() { if (ready) PlaySound(sndKill); }

    void cleanup()
    {
        if (!ready) return;
        UnloadSound(sndRoll);
        UnloadSound(sndMove);
        UnloadSound(sndKill);
        CloseAudioDevice();
        ready = false;
    }
};

SoundSystem gSounds;

//  CLASS: Dice
class Dice
{
    int  val;
    bool rolled;
public:
    Dice() : val(0), rolled(false) {}
    int  roll()      { val = rand()%6+1; rolled=true; return val; }
    int  getValue()  const { return val;    }
    bool hasRolled() const { return rolled; }
    void reset()     { val=0; rolled=false; }
};

//  CLASS: Token
class Token
{
    int  pathIndex;
    bool atBase;
    bool finished;
    int  homeStep;
    bool inHomeStretch;
public:
    Token() : pathIndex(-1), atBase(true), finished(false),
              homeStep(0), inHomeStretch(false) {}

    int  getPathIndex()    const { return pathIndex;     }
    bool isAtBase()        const { return atBase;        }
    bool isFinished()      const { return finished;      }
    bool isInHomeStretch() const { return inHomeStretch; }
    int  getHomeStep()     const { return homeStep;      }

    void enterBoard(int si) { atBase=false; pathIndex=si; }

    bool moveForward(int steps, int homeEntryIdx, bool hasKilled)
    {
        if (atBase || finished) return false;
        if (inHomeStretch)
        {
            if (homeStep+steps>6) return false;
            homeStep+=steps;
            if (homeStep==6) finished=true;
            return true;
        }
        int distToEntry = (homeEntryIdx - pathIndex + PATH_LEN) % PATH_LEN;
        if (distToEntry==0) distToEntry=PATH_LEN;
        if (steps < distToEntry)
        {
            pathIndex = (pathIndex+steps) % PATH_LEN;
            return true;
        }
        else
        {
            if (!hasKilled) return false;
            int overshoot = steps - distToEntry;
            if (overshoot>6) return false;
            inHomeStretch=true;
            homeStep=overshoot;
            pathIndex=-1;
            if (homeStep==6) finished=true;
            return true;
        }
    }

    void sendToBase()
    {
        pathIndex=-1; atBase=true;
        inHomeStretch=false; homeStep=0; finished=false;
    }
};

//  CLASS: Player
class Player
{
protected:
    bool               hasKilled;
    std::string        name;
    std::vector<Token> tokens;
    Color              color;
    int                startIdx;
    int                homeEntryIdx;
    int                finishedCount;
    int                id;
public:
    Player(const std::string &n, Color c, int si, int hi, int pid)
        : name(n), color(c), startIdx(si), homeEntryIdx(hi),
          finishedCount(0), id(pid), hasKilled(false)
    { tokens.resize(2); }

    virtual ~Player() {}
    virtual int chooseToken(int diceVal) = 0;

    const std::string &getName()          const { return name;  }
    Color              getColor()         const { return color; }
    int                getId()            const { return id;    }
    int                getFinishedCount() const { return finishedCount; }
    Token             &getToken(int i)          { return tokens[i]; }
    int                getTokenCount()    const { return (int)tokens.size(); }
    int                getStartIdx()      const { return startIdx; }
    bool               hasKill()          const { return hasKilled; }
    void               setKill()                { hasKilled=true; }

    bool moveToken(int ti, int val)
    {
        Token &t = tokens[ti];
        if (t.isAtBase())
        {
            if (val==6) { t.enterBoard(startIdx); return true; }
            return false;
        }
        bool ok = t.moveForward(val, homeEntryIdx, hasKilled);
        if (ok && t.isFinished()) finishedCount++;
        return ok;
    }

    bool hasWon() const { return finishedCount>=(int)tokens.size(); }

    bool hasValidMove(int val) const
    {
        for (const Token &t : tokens)
        {
            if (t.isFinished()) continue;
            if (t.isAtBase() && val==6) return true;
            if (!t.isAtBase())
            {
                if (t.isInHomeStretch())
                { if (t.getHomeStep()+val<=6) return true; }
                else
                {
                    int dist = (homeEntryIdx - t.getPathIndex() + PATH_LEN) % PATH_LEN;
                    if (dist==0) dist=PATH_LEN;
                    int over = (val<=dist) ? 0 : val-dist;
                    if (over>6) continue;
                    if (over>0 && !hasKilled) continue;
                    return true;
                }
            }
        }
        return false;
    }
};

//  HumanPlayer
class HumanPlayer : public Player
{
public:
    HumanPlayer(const std::string &n, Color c, int si, int hi)
        : Player(n,c,si,hi,0) {}
    int chooseToken(int) override { return -1; }
};

//  AIPlayer
class AIPlayer : public Player
{
public:
    AIPlayer(const std::string &n, Color c, int si, int hi)
        : Player(n,c,si,hi,1) {}

    int chooseToken(int val) override
    {
        if (val==6)
            for (int i=0;i<(int)tokens.size();i++)
                if (tokens[i].isAtBase()) return i;

        int best=-1, bestProg=-1;
        for (int i=0;i<(int)tokens.size();i++)
        {
            Token &t=tokens[i];
            if (t.isAtBase()||t.isFinished()) continue;
            if (t.isInHomeStretch())
            { if (t.getHomeStep()+val>6) continue; }
            else
            {
                int dist=(homeEntryIdx-t.getPathIndex()+PATH_LEN)%PATH_LEN;
                if (dist==0) dist=PATH_LEN;
                int over=(val<=dist)?0:val-dist;
                if (over>6) continue;
                if (over>0 && !hasKill()) continue;
            }
            int prog = t.isInHomeStretch()
                ? PATH_LEN+t.getHomeStep()
                : (t.getPathIndex()-startIdx+PATH_LEN)%PATH_LEN;
            if (prog>bestProg) { bestProg=prog; best=i; }
        }
        return best;
    }
};

//  CLASS: Board
class Board
{
public:
    void draw()
    {
        for (int r=0;r<15;r++)
        for (int c=0;c<15;c++)
        {
            Color cell={245,245,220,255};

            if (c<6 && r>8) cell={210,50,50,255};
            if (c<6 && r<6) cell={50,100,210,255};
            if (c>8 && r<6) cell={50,190,70,255};
            if (c>8 && r>8) cell={220,190,30,255};

            if (r==6||r==8) cell=WHITE;
            if (c==6||c==8) cell=WHITE;

            if (c==7 && r>=8 && r<=13) cell={210,70,70,255};
            if (r==7 && c>=1 && c<=6)  cell={80,150,220,255};
            if (c==7 && r>=1 && r<=6)  cell={50,190,70,255};
            if (r==7 && c>=8 && c<=13) cell={220,190,30,255};

            if (c>=6&&c<=8&&r>=6&&r<=8) cell={200,200,200,255};

            DrawRectangle(BOARD_OFF+c*CELL_SIZE, BOARD_OFF+r*CELL_SIZE,
                          CELL_SIZE, CELL_SIZE, cell);
            DrawRectangleLines(BOARD_OFF+c*CELL_SIZE, BOARD_OFF+r*CELL_SIZE,
                               CELL_SIZE, CELL_SIZE, {160,160,160,255});
        }

        // Safe squares — purple with star
        Color safeCol = {140, 60, 200, 255};
        for (int i=0; i<SAFE_SQUARES_LEN; i++)
        {
            int idx = SAFE_SQUARES[i];
            int c   = PATH_COL[idx];
            int r   = PATH_ROW[idx];
            DrawRectangle(BOARD_OFF+c*CELL_SIZE+1, BOARD_OFF+r*CELL_SIZE+1,
                          CELL_SIZE-2, CELL_SIZE-2, safeCol);
            DrawRectangleLines(BOARD_OFF+c*CELL_SIZE, BOARD_OFF+r*CELL_SIZE,
                               CELL_SIZE, CELL_SIZE, {160,160,160,255});
            DrawText(" ", BOARD_OFF+c*CELL_SIZE+16, BOARD_OFF+r*CELL_SIZE+12, 22, WHITE);
        }

        DrawText("RED",    BOARD_OFF+30,            BOARD_OFF+9*CELL_SIZE+10, 18, WHITE);
        DrawText("BLUE",   BOARD_OFF+30,            BOARD_OFF+30,             18, WHITE);
        DrawText("GREEN",  BOARD_OFF+9*CELL_SIZE+10,BOARD_OFF+30,             18, WHITE);
        DrawText("YELLOW", BOARD_OFF+9*CELL_SIZE+5, BOARD_OFF+9*CELL_SIZE+10, 14, {60,50,0,255});
    }

    void drawToken(const Token &tok, Color col, int playerStartIdx, int tokenIdx)
    {
        float r   = 13.f;
        float off = tokenIdx==0 ? -8.f : 8.f;
        Vector2 p;

        if (tok.isFinished())
        {
            p=CellCentre(7,7);
            p.x += (playerStartIdx==0)?off:0;
            p.y += (playerStartIdx==0)?0:off;
        }
        else if (tok.isAtBase())
        {
            if (playerStartIdx==0)
                p={(float)(BOARD_OFF+(1+tokenIdx*2)*CELL_SIZE+CELL_SIZE/2),
                   (float)(BOARD_OFF+11*CELL_SIZE+CELL_SIZE/2)};
            else
                p={(float)(BOARD_OFF+(1+tokenIdx*2)*CELL_SIZE+CELL_SIZE/2),
                   (float)(BOARD_OFF+2*CELL_SIZE+CELL_SIZE/2)};
        }
        else if (tok.isInHomeStretch())
        {
            int s=tok.getHomeStep(); if(s<0)s=0; if(s>5)s=5;
            if (playerStartIdx==0)
                p=CellCentre(RED_HOME_C[s],RED_HOME_R[s]);
            else
                p=CellCentre(BLUE_HOME_C[s],BLUE_HOME_R[s]);
            p.x+=off*0.4f; p.y+=off*0.4f;
        }
        else
        {
            int idx=tok.getPathIndex();
            if (idx<0||idx>=PATH_LEN) return;
            p=CellCentre(PATH_COL[idx],PATH_ROW[idx]);
            p.x+=off*0.4f; p.y+=off*0.4f;
        }

        DrawCircleV(p,r,col);
        DrawCircleLines((int)p.x,(int)p.y,(int)r,BLACK);
        char lbl[4]; sprintf(lbl,"T%d",tokenIdx+1);
        DrawText(lbl,(int)p.x-9,(int)p.y-6,11,WHITE);
    }

    void drawDice(int v,int x,int y)
    {
        DrawRectangle(x,y,64,64,WHITE);
        DrawRectangleLines(x,y,64,64,DARKGRAY);
        Color d=BLACK;
        switch(v){
        case 1: DrawCircle(x+32,y+32,6,d); break;
        case 2: DrawCircle(x+16,y+16,6,d); DrawCircle(x+48,y+48,6,d); break;
        case 3: DrawCircle(x+16,y+16,6,d); DrawCircle(x+32,y+32,6,d); DrawCircle(x+48,y+48,6,d); break;
        case 4: DrawCircle(x+16,y+16,6,d); DrawCircle(x+48,y+16,6,d);
                DrawCircle(x+16,y+48,6,d); DrawCircle(x+48,y+48,6,d); break;
        case 5: DrawCircle(x+16,y+16,6,d); DrawCircle(x+48,y+16,6,d); DrawCircle(x+32,y+32,6,d);
                DrawCircle(x+16,y+48,6,d); DrawCircle(x+48,y+48,6,d); break;
        case 6: DrawCircle(x+16,y+12,6,d); DrawCircle(x+48,y+12,6,d);
                DrawCircle(x+16,y+32,6,d); DrawCircle(x+48,y+32,6,d);
                DrawCircle(x+16,y+52,6,d); DrawCircle(x+48,y+52,6,d); break;
        }
    }
};

// ============================================================
//  FileHandler
// ============================================================
class FileHandler
{
    std::string fn;
public:
    FileHandler(const std::string &f):fn(f){}
    void saveResult(const std::string &w)
    {
        try{
            std::ofstream o(fn,std::ios::app);
            if(!o.is_open()) return;
            time_t now=time(nullptr); char buf[64];
            strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",localtime(&now));
            o<<"["<<buf<<"] Winner: "<<w<<"\n";
        }catch(...){}
    }
    std::string loadResults()
    {
        std::string r;
        try{
            std::ifstream i(fn);
            if(!i.is_open()) return "No records found.";
            std::string line;
            while(std::getline(i,line)) r+=line+"\n";
        }catch(...){}
        return r;
    }
};

//  GameState
enum class GS { MENU, HUMAN_ROLLING, HUMAN_PICK, AI_TURN, GAME_OVER, RESULTS };

//  CLASS: Game
class Game
{
    Board        board;
    Dice         dice;
    HumanPlayer *human;
    AIPlayer    *computer;
    FileHandler  fh;
    GS           state;
    int          curPlayer;
    std::string  msg;
    int          aiDelay;
    std::string  pastRes;
    int          lastRoll;

    Rectangle btnRoll    = {790,280,60,38};
    Rectangle btnMenu    = {790,330,60,38};
    Rectangle btnResults = {790,380,60,38};

    void checkKill(Player *atk)
    {
        Player *opp = (atk==(Player*)human) ? (Player*)computer : (Player*)human;
        for (int i=0;i<atk->getTokenCount();i++)
        {
            Token &a=atk->getToken(i);
            if (a.isAtBase()||a.isFinished()||a.isInHomeStretch()) continue;
            if (isSafeSquare(a.getPathIndex())) continue;
            for (int j=0;j<opp->getTokenCount();j++)
            {
                Token &b=opp->getToken(j);
                if (b.isAtBase()||b.isFinished()||b.isInHomeStretch()) continue;
                if (isSafeSquare(b.getPathIndex())) continue;
                if (a.getPathIndex()==b.getPathIndex())
                {
                    b.sendToBase();
                    atk->setKill();
                    gSounds.playKill();
                }
            }
        }
    }

    int getClickedToken(Vector2 mouse)
    {
        for (int i=0;i<human->getTokenCount();i++)
        {
            Token &t=human->getToken(i);
            if (t.isFinished()) continue;
            Vector2 p;
            if (t.isAtBase())
                p={(float)(BOARD_OFF+(1+i*2)*CELL_SIZE+CELL_SIZE/2),
                   (float)(BOARD_OFF+11*CELL_SIZE+CELL_SIZE/2)};
            else if (t.isInHomeStretch())
            {
                int s=t.getHomeStep(); if(s<0)s=0; if(s>5)s=5;
                p=CellCentre(RED_HOME_C[s],RED_HOME_R[s]);
            }
            else
            {
                int idx=t.getPathIndex();
                if (idx<0||idx>=PATH_LEN) continue;
                p=CellCentre(PATH_COL[idx],PATH_ROW[idx]);
            }
            float dx=mouse.x-p.x, dy=mouse.y-p.y;
            if (dx*dx+dy*dy<=22*22) return i;
        }
        return -1;
    }

    void newGame()
    {
        delete human; delete computer;
        human    = new HumanPlayer("Player (Red)",    RED,  0,  RED_HOME_ENTRY);
        computer = new AIPlayer   ("Computer (Blue)", BLUE, 13, BLUE_HOME_ENTRY);
        dice.reset(); lastRoll=0; curPlayer=0;
        state=GS::MENU; msg="Welcome to Ludo!";
    }

public:
    Game():fh("ludo_results.txt"),state(GS::MENU),
           curPlayer(0),msg("Welcome!"),aiDelay(0),lastRoll(0)
    {
        human    = new HumanPlayer("Player (Red)",    RED,  0,  RED_HOME_ENTRY);
        computer = new AIPlayer   ("Computer (Blue)", BLUE, 13, BLUE_HOME_ENTRY);
    }
    ~Game(){ delete human; delete computer; }

    void update()
    {
        Vector2 mouse=GetMousePosition();
        bool    click=IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        switch(state)
        {
        case GS::MENU:
            if(click){
                if(CheckCollisionPointRec(mouse,{280,340,200,50}))
                { state=GS::HUMAN_ROLLING; msg="Your turn — click ROLL."; }
                if(CheckCollisionPointRec(mouse,{280,410,200,50}))
                { pastRes=fh.loadResults(); state=GS::RESULTS; }
            }
            break;

        case GS::HUMAN_ROLLING:
            if(click && CheckCollisionPointRec(mouse,btnRoll))
            {
                lastRoll=dice.roll();
                gSounds.playRoll();
                msg="You rolled "+std::to_string(lastRoll);
                if(!human->hasValidMove(lastRoll))
                { msg+=" — no moves. AI's turn."; dice.reset(); curPlayer=1; aiDelay=90; state=GS::AI_TURN; }
                else
                { msg+=" — click your token."; state=GS::HUMAN_PICK; }
            }
            break;

        case GS::HUMAN_PICK:
        {
            if(!click) break;
            int ti=getClickedToken(mouse);
            if(ti==-1) break;
            if(!human->moveToken(ti,lastRoll))
            { msg="Can't move that token — try the other one."; break; }
            gSounds.playMove();
            checkKill((Player*)human);
            dice.reset();
            if(human->hasWon())
            { state=GS::GAME_OVER; msg="YOU WIN! Congratulations!"; fh.saveResult(human->getName()); break; }
            if(lastRoll==6)
            { lastRoll=0; state=GS::HUMAN_ROLLING; msg="Rolled 6 — bonus turn!"; }
            else
            { lastRoll=0; curPlayer=1; aiDelay=90; state=GS::AI_TURN; msg="Computer's turn..."; }
            break;
        }

        case GS::AI_TURN:
            if(--aiDelay>0) break;
            {
                lastRoll=dice.roll();
                gSounds.playRoll();
                msg="Computer rolled "+std::to_string(lastRoll);
                if(!computer->hasValidMove(lastRoll))
                { msg+=" — no moves. Your turn."; dice.reset(); lastRoll=0; curPlayer=0; state=GS::HUMAN_ROLLING; break; }
                int ti=computer->chooseToken(lastRoll);
                if(ti!=-1 && computer->moveToken(ti,lastRoll))
                {
                    gSounds.playMove();
                    checkKill((Player*)computer);
                    msg+=" — AI moved T"+std::to_string(ti+1);
                }
                dice.reset();
                if(computer->hasWon())
                { lastRoll=0; state=GS::GAME_OVER; msg="Computer wins! Better luck next time."; fh.saveResult(computer->getName()); break; }
                if(lastRoll==6)
                { lastRoll=0; aiDelay=60; msg+=" (bonus turn)"; }
                else
                { lastRoll=0; curPlayer=0; state=GS::HUMAN_ROLLING; msg="Your turn — click ROLL."; }
            }
            break;

        case GS::GAME_OVER:
            if(click && CheckCollisionPointRec(mouse,btnMenu)) newGame();
            break;

        case GS::RESULTS:
            if(click) state=GS::MENU;
            break;
        }
    }

    void draw()
    {
        BeginDrawing();
        ClearBackground({30,30,40,255});

        switch(state)
        {
        case GS::MENU:
{
    const char *title = "LUDO GAME";
    const char *sub1  = "BCS-2E | OOP Project";
    const char *sub2  = "25K-0921 | 25K-0544";

    int titleFont = 60;
    int sub1Font  = 22;
    int sub2Font  = 20;

    int titleX = (SCREEN_W - MeasureText(title, titleFont)) / 2;
    int sub1X  = (SCREEN_W - MeasureText(sub1, sub1Font)) / 2;
    int sub2X  = (SCREEN_W - MeasureText(sub2, sub2Font)) / 2;

    DrawText(title, titleX, 120, titleFont, GOLD);
    DrawText(sub1,  sub1X,  205, sub1Font, LIGHTGRAY);
    DrawText(sub2,  sub2X,  238, sub2Font, LIGHTGRAY);

    // Play button
    Rectangle playBtn = { (SCREEN_W - 220) / 2.0f, 340, 220, 55 };
    DrawRectangleRounded(playBtn, 0.3f, 8, GREEN);
    DrawText("PLAY GAME",
             playBtn.x + (playBtn.width - MeasureText("PLAY GAME", 24)) / 2,
             playBtn.y + 15,
             24,
             WHITE);

    // Results button
    Rectangle resultsBtn = { (SCREEN_W - 220) / 2.0f, 415, 220, 55 };
    DrawRectangleRounded(resultsBtn, 0.3f, 8, DARKBLUE);
    DrawText("PAST RESULTS",
             resultsBtn.x + (resultsBtn.width - MeasureText("PAST RESULTS", 22)) / 2,
             resultsBtn.y + 16,
             22,
             WHITE);

    // Safe square legend
    int legendX = (SCREEN_W - 250) / 2;
    DrawRectangle(legendX, 505, 22, 22, {140,60,200,255});
    DrawText("*", legendX + 6, 507, 16, WHITE);
    DrawText("= Safe square (no kill)", legendX + 32, 508, 18, LIGHTGRAY);

    break;
}

        case GS::HUMAN_ROLLING:
        case GS::HUMAN_PICK:
        case GS::AI_TURN:
            board.draw();
            for(int i=0;i<human->getTokenCount();i++)
                board.drawToken(human->getToken(i),   RED,  0,  i);
            for(int i=0;i<computer->getTokenCount();i++)
                board.drawToken(computer->getToken(i), BLUE, 13, i);

            DrawRectangle(780,0,80,800,{18,18,28,255});

            if(dice.getValue()>0) board.drawDice(dice.getValue(),784,200);
            else{ DrawRectangle(784,200,64,64,DARKGRAY); DrawText("?",802,220,30,WHITE); }

            DrawRectangleRec(btnRoll,   (state==GS::HUMAN_ROLLING)?GREEN:DARKGRAY);
            DrawText("ROLL",796,291,17,WHITE);
            // DrawRectangleRec(btnMenu,   MAROON);
            // DrawText("MENU",796,341,17,WHITE);
            // DrawRectangleRec(btnResults,DARKBLUE);
            // DrawText("RES", 803,391,15,WHITE);

            DrawText(curPlayer==0?"YOUR":"  AI",784,440,16,YELLOW);
            DrawText("TURN",                     790,458,16,YELLOW);

            DrawRectangle(0,762,780,38,{18,18,28,220});
            DrawText(msg.c_str(),8,773,16,WHITE);
            break;

        case GS::GAME_OVER:
            DrawText("GAME OVER", 190,200,60,GOLD);
            DrawText(msg.c_str(), 110,300,24,WHITE);
            DrawRectangleRec(btnMenu,GREEN);
            DrawText("MENU",796,341,17,WHITE);
            DrawText("Click MENU to restart",230,500,20,LIGHTGRAY);
            break;

        case GS::RESULTS:
            DrawText("PAST RESULTS",200,70,40,GOLD);
            DrawText(pastRes.empty()?"No records.":pastRes.c_str(),50,140,16,WHITE);
            DrawText("Click anywhere to go back.",200,720,20,LIGHTGRAY);
            break;
        }

        DrawFPS(6,5);
        EndDrawing();
    }

    void run()
    {
        InitWindow(SCREEN_W,SCREEN_H,"Ludo — OOP Project BCS-2E");
        gSounds.init();
        SetTargetFPS(60);
        while(!WindowShouldClose()){ update(); draw(); }
        gSounds.cleanup();
        CloseWindow();
    }
};
//  main
int main()
{
    srand((unsigned)time(nullptr));
    try{ Game g; g.run(); }
    catch(const std::exception &e)
    { std::cerr<<"Fatal: "<<e.what()<<std::endl; return 1; }
    return 0;
}