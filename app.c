#include "app.h"
#include "screen_page1.h"
#include "screen_page2.h"
#include "screen_page3.h"
#include "screen_page4.h"
#include "screen_page5.h"
#include "screen_page6.h"
#include "time_service.h"
#include "lvgl/lvgl.h"

static int s_current_page_id = 1;

/* Tároljuk az aktuális animációs irányt, hogy a load_page tudja, merre mozduljon */
static lv_screen_load_anim_t s_anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;

/* ---- Beépített Gesture Handler ---- */
static void gesture_handler(lv_event_t* e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    int next_id = s_current_page_id;

    if (dir == LV_DIR_LEFT) {
        /* Balra húzás -> Következő oldal jön JOBBRÓL BALRA */
        s_anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
        next_id = s_current_page_id + 1;
        if (next_id > 6) next_id = 1;
        app_load_page(next_id);
    }
    else if (dir == LV_DIR_RIGHT) {
        /* Jobbra húzás -> Előző oldal jön BALRÓL JOBBRA */
        s_anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        next_id = s_current_page_id - 1;
        if (next_id < 1) next_id = 6;
        app_load_page(next_id);
    }
}

/* ---- Page factory ---- */
static lv_obj_t* create_page_by_id(int id)
{
    switch (id) {
    case 1: return screen_page1_create();
    case 2: return screen_page2_create();
    case 3: return screen_page3_create();
    case 4: return screen_page4_create();
    case 5: return screen_page5_create();
    case 6: return screen_page6_create();
    default: return screen_page1_create();
    }
}

/* ---- Page loader ---- */
void app_load_page(int id)
{
    if (id < 1) id = 1;
    if (id > 6) id = 6;

    /* Új oldal létrehozása */
    lv_obj_t* next_scr = create_page_by_id(id);
    s_current_page_id = id;

    /* Gesture esemény hozzáadása az ÚJ képernyőhöz */
    lv_obj_add_event_cb(next_scr, gesture_handler, LV_EVENT_GESTURE, NULL);

    /* Fontos: A képernyőnek kattinthatónak kell lennie a gesture-höz */
    lv_obj_add_flag(next_scr, LV_OBJ_FLAG_CLICKABLE);

    /* ANIMÁCIÓ BEÁLLÍTÁSA:
       - 2. paraméter: s_anim_type (dinamikusan változik a húzás irányától függően)
       - 3. paraméter: 250 <--- ITT ÁLLÍTHATOD A SEBESSÉGET (milliszekundum)
                       Kisebb szám = gyorsabb váltás, nagyobb szám = lassabb úszás.
    */
    lv_screen_load_anim(next_scr, s_anim_type, 250, 0, true);
}

int app_get_current_page_id(void)
{
    return s_current_page_id;
}

void app_init(void)
{
    time_service_init();
    time_service_sync();

    /* Alapértelmezett irány az első induláshoz */
    s_anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;

    /* Első oldal betöltése */
    app_load_page(1);
}