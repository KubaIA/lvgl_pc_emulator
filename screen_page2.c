#include "screen_page2.h"
#include "ui_common.h"
#include "app.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "cJSON.h"

#include "pictures/img_cpu.h"
#include "pictures/img_ram.h"
#include "pictures/img_hdd.h"
#include "pictures/img_net.h"

#pragma comment(lib, "ws2_32.lib")

#define BAR_H 444
#define BAR_W 180
#define MAX_IO_KBS 500000.0    // SATA SSD (~500 MB/s) | PCIe 3.0 NVMe SSD (~3500 MB/s) | PCIe 4.0 NVMe SSD (~7500 MB/s)
#define MAX_NET_KBS 12500.0    // Ha 100 Mbps a neted, akkor 12500.0 | Ha 1 Gbps (Gigabit), akkor 125000.0

static lv_obj_t* s_scr_page2 = NULL;

/* --- Labelek (Összevont disk százalékkal) --- */
static lv_obj_t* pc_name_tag, * pc_name_val;
static lv_obj_t* cpu_type, * cpu_val, * cpu_pct;
static lv_obj_t* ram_size, * ram_val, * ram_pct;
static lv_obj_t* disk_drive, * disk_val;
static lv_obj_t* disk_io_tag, * disk_io_val, * disk_io_unit;
static lv_obj_t* net_io_tag, * net_io_val, * net_io_unit;

/* --- Ikonok --- */
static lv_obj_t* icon_cpu;
static lv_obj_t* icon_ram;
static lv_obj_t* icon_disk;
static lv_obj_t* icon_net;

static lv_obj_t* f_cpu, * f_ram, * f_disk, * f_net;

static SOCKET udp_socket = INVALID_SOCKET;
static HANDLE receive_thread = NULL;
static int ws_initialized = 0;
static volatile bool s_run_thread = false;

static void split_dynamic_unit(double kbs_val, char* out_val, char* out_unit) {
    const char* units[] = { "B/s", "KB/s", "MB/s", "GB/s" };
    int unit_index = 1;
    double val = kbs_val;

    if (val > 0 && val < 1.0) {
        val *= 1024.0;
        unit_index = 0;
    }
    else {
        while (val >= 1000.0 && unit_index < 3) {
            val /= 1024.0;
            unit_index++;
        }
    }
    snprintf(out_val, 16, "%06.2f", val);
    strcpy(out_unit, units[unit_index]);
}

static void update_fill_obj(lv_obj_t* obj, double current, double max) {
    if (!obj || !lv_obj_is_valid(obj)) return;
    if (current < 0) current = 0;
    if (current > max) current = max;
    lv_coord_t fill_h = (lv_coord_t)((current / max) * BAR_H);
    lv_obj_set_size(obj, lv_obj_get_width(obj), fill_h);
    lv_obj_set_pos(obj, lv_obj_get_x(obj), BAR_H - fill_h);
}

static void process_json(const char* json_str)
{
    cJSON* root = cJSON_Parse(json_str);
    if (!root) { free((void*)json_str); return; }
    if (s_scr_page2 == NULL || !lv_obj_is_valid(s_scr_page2)) {
        cJSON_Delete(root); free((void*)json_str); return;
    }

    cJSON* computer = cJSON_GetObjectItem(root, "computer");
    if (cJSON_IsString(computer) && pc_name_val) {
        lv_label_set_text(pc_name_val, computer->valuestring);
    }

    /* CPU */
    cJSON* cpu = cJSON_GetObjectItem(root, "cpu");
    if (cJSON_IsObject(cpu)) {
        cJSON* name = cJSON_GetObjectItem(cpu, "name");
        cJSON* usage = cJSON_GetObjectItem(cpu, "usage_pct");
        if (usage) {
            int val_int = (int)(usage->valuedouble);
            int val_frac = (int)((usage->valuedouble - val_int) * 10);
            if (cpu_type && name) lv_label_set_text(cpu_type, name->valuestring);
            if (cpu_val) lv_label_set_text_fmt(cpu_val, "%d.%d", val_int, abs(val_frac));
            if (cpu_pct) lv_label_set_text(cpu_pct, "%");
            update_fill_obj(f_cpu, usage->valuedouble, 100.0);
        }
    }

    /* RAM */
    cJSON* ram = cJSON_GetObjectItem(root, "ram");
    if (cJSON_IsObject(ram)) {
        cJSON* total = cJSON_GetObjectItem(ram, "total_gb");
        cJSON* used = cJSON_GetObjectItem(ram, "used_pct");
        if (total && used) {
            int t_int = (int)(total->valuedouble);
            int t_frac = (int)((total->valuedouble - t_int) * 10);
            if (ram_size) lv_label_set_text_fmt(ram_size, "%d.%d GB", t_int, abs(t_frac));
            if (ram_val) lv_label_set_text_fmt(ram_val, "%d", (int)used->valuedouble);
            if (ram_pct) lv_label_set_text(ram_pct, "%");
            update_fill_obj(f_ram, used->valuedouble, 100.0);
        }
    }

    /* Disk */
    cJSON* disk = cJSON_GetObjectItem(root, "disk");
    if (cJSON_IsObject(disk)) {
        cJSON* total = cJSON_GetObjectItem(disk, "total_gb");
        cJSON* used = cJSON_GetObjectItem(disk, "used_pct");
        if (total && used) {
            int d_int = (int)(total->valuedouble);
            int d_frac = (int)((total->valuedouble - d_int) * 10);
            if (disk_drive) lv_label_set_text_fmt(disk_drive, "C: %d.%d GB", d_int, abs(d_frac));
            if (disk_val) lv_label_set_text_fmt(disk_val, "%d%%", (int)used->valuedouble);
        }
    }

    /* Disk IO */
    cJSON* disk_io = cJSON_GetObjectItem(root, "disk_io");
    if (cJSON_IsObject(disk_io)) {
        char val_buf[16], unit_buf[16];
        double r = cJSON_GetObjectItem(disk_io, "read_kb_s") ? cJSON_GetObjectItem(disk_io, "read_kb_s")->valuedouble : 0;
        double w = cJSON_GetObjectItem(disk_io, "write_kb_s") ? cJSON_GetObjectItem(disk_io, "write_kb_s")->valuedouble : 0;
        double total_io = r + w;
        split_dynamic_unit(total_io, val_buf, unit_buf);
        if (disk_io_val) lv_label_set_text(disk_io_val, val_buf);
        if (disk_io_unit) lv_label_set_text(disk_io_unit, unit_buf);
        update_fill_obj(f_disk, total_io, MAX_IO_KBS);
    }

    /* Net */
    cJSON* net = cJSON_GetObjectItem(root, "net");
    if (cJSON_IsObject(net)) {
        char val_buf[16], unit_buf[16];
        double rx = cJSON_GetObjectItem(net, "rx_kb_s") ? cJSON_GetObjectItem(net, "rx_kb_s")->valuedouble : 0;
        double tx = cJSON_GetObjectItem(net, "tx_kb_s") ? cJSON_GetObjectItem(net, "tx_kb_s")->valuedouble : 0;
        double total_net = rx + tx;
        split_dynamic_unit(total_net, val_buf, unit_buf);
        if (net_io_val) lv_label_set_text(net_io_val, val_buf);
        if (net_io_unit) lv_label_set_text(net_io_unit, unit_buf);
        update_fill_obj(f_net, total_net, MAX_NET_KBS);
    }

    cJSON_Delete(root);
    free((void*)json_str);
}

static DWORD WINAPI udp_receive_thread(LPVOID arg)
{
    if (!ws_initialized) { WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData); ws_initialized = 1; }
    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(udp_socket, (struct sockaddr*)&addr, sizeof(addr));
    s_run_thread = true;
    while (s_run_thread) {
        char buffer[2048];
        int len = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
        if (len > 0 && s_run_thread) { buffer[len] = '\0'; lv_async_call((lv_async_cb_t)process_json, _strdup(buffer)); }
    }
    return 0;
}

static void screen_page2_delete_cb(lv_event_t* e)
{
    s_run_thread = false;
    if (udp_socket != INVALID_SOCKET) { closesocket(udp_socket); udp_socket = INVALID_SOCKET; }
    if (receive_thread) { WaitForSingleObject(receive_thread, 500); CloseHandle(receive_thread); receive_thread = NULL; }
    s_scr_page2 = NULL;
}

static lv_obj_t* create_bar_group(lv_obj_t* scr, int x, lv_color_t c1, lv_color_t c2, lv_obj_t** f1, lv_obj_t** f2, bool twin) {
    lv_obj_t* bg = lv_obj_create(scr);
    lv_obj_set_size(bg, BAR_W, BAR_H);
    lv_obj_set_pos(bg, x, 16);
    lv_obj_set_style_bg_color(bg, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_30, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_set_style_radius(bg, 10, 0);
    lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    int w = twin ? (BAR_W / 2) - 5 : BAR_W;
    *f1 = lv_obj_create(bg);
    lv_obj_set_size(*f1, w, 0);
    lv_obj_set_pos(*f1, 0, BAR_H);
    lv_obj_set_style_bg_color(*f1, c1, 0);
    lv_obj_set_style_radius(*f1, 10, 0);
    lv_obj_set_style_border_width(*f1, 0, 0);

    if (twin && f2) {
        *f2 = lv_obj_create(bg);
        lv_obj_set_size(*f2, w, 0);
        lv_obj_set_pos(*f2, BAR_W - w, BAR_H);
        lv_obj_set_style_bg_color(*f2, c2, 0);
        lv_obj_set_style_radius(*f2, 10, 0);
        lv_obj_set_style_border_width(*f2, 0, 0);
    }
    return bg;
}

lv_obj_t* screen_page2_create(void)
{
    s_scr_page2 = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_page2, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(s_scr_page2, LV_OPA_COVER, 0);
    lv_obj_add_event_cb(s_scr_page2, screen_page2_delete_cb, LV_EVENT_DELETE, NULL);

    int step = BAR_W + 16;
    lv_obj_t* cpu_bar = create_bar_group(s_scr_page2, 16, lv_palette_main(LV_PALETTE_GREEN), lv_color_black(), &f_cpu, NULL, false);
    lv_obj_t* ram_bar = create_bar_group(s_scr_page2, 16 + step, lv_palette_main(LV_PALETTE_BLUE), lv_color_black(), &f_ram, NULL, false);
    lv_obj_t* disk_bar = create_bar_group(s_scr_page2, 16 + step * 2, lv_palette_main(LV_PALETTE_ORANGE), lv_color_black(), &f_disk, NULL, false);
    lv_obj_t* net_bar = create_bar_group(s_scr_page2, 16 + step * 3, lv_palette_main(LV_PALETTE_CYAN), lv_color_black(), &f_net, NULL, false);

    /* --- Ikonok létrehozása (Page4 logika) --- */
    icon_cpu = lv_img_create(cpu_bar);
    lv_img_set_src(icon_cpu, &img_cpu);
    lv_obj_set_style_image_recolor(icon_cpu, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_cpu, LV_OPA_COVER, 0);
    lv_obj_align(icon_cpu, LV_ALIGN_TOP_MID, 0, 100);

    icon_ram = lv_img_create(ram_bar);
    lv_img_set_src(icon_ram, &img_ram);
    lv_obj_set_style_image_recolor(icon_ram, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_ram, LV_OPA_COVER, 0);
    lv_obj_align(icon_ram, LV_ALIGN_TOP_MID, 0, 100);

    icon_disk = lv_img_create(disk_bar);
    lv_img_set_src(icon_disk, &img_hdd);
    lv_obj_set_style_image_recolor(icon_disk, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_disk, LV_OPA_COVER, 0);
    lv_obj_align(icon_disk, LV_ALIGN_TOP_MID, 0, 100);

    icon_net = lv_img_create(net_bar);
    lv_img_set_src(icon_net, &img_net);
    lv_obj_set_style_image_recolor(icon_net, lv_color_white(), 0);
    lv_obj_set_style_image_recolor_opa(icon_net, LV_OPA_COVER, 0);
    lv_obj_align(icon_net, LV_ALIGN_TOP_MID, 0, 100);

    /* --- PC NAME --- */
    pc_name_tag = lv_label_create(cpu_bar);
    lv_label_set_text(pc_name_tag, "PC Name:");
    lv_obj_set_style_text_font(pc_name_tag, &lv_font_montserrat_24, 0);
    lv_obj_align(pc_name_tag, LV_ALIGN_TOP_MID, 0, 10);

    pc_name_val = lv_label_create(cpu_bar);
    lv_obj_set_style_text_font(pc_name_val, &lv_font_montserrat_24, 0);
    lv_obj_align(pc_name_val, LV_ALIGN_TOP_MID, 0, 40);

    /* --- CPU --- */
    cpu_type = lv_label_create(cpu_bar); lv_obj_set_style_text_font(cpu_type, &lv_font_montserrat_24, 0); lv_obj_align(cpu_type, LV_ALIGN_CENTER, 0, 50);
    cpu_val = lv_label_create(cpu_bar);  lv_obj_set_style_text_font(cpu_val, &lv_font_montserrat_48, 0);  lv_obj_align(cpu_val, LV_ALIGN_CENTER, 0, 90);
    cpu_pct = lv_label_create(cpu_bar);  lv_obj_set_style_text_font(cpu_pct, &lv_font_montserrat_32, 0);  lv_obj_align(cpu_pct, LV_ALIGN_CENTER, 0, 130);

    /* --- RAM --- */
    ram_size = lv_label_create(ram_bar); lv_obj_set_style_text_font(ram_size, &lv_font_montserrat_24, 0); lv_obj_align(ram_size, LV_ALIGN_CENTER, 0, 50);
    ram_val = lv_label_create(ram_bar);  lv_obj_set_style_text_font(ram_val, &lv_font_montserrat_48, 0);  lv_obj_align(ram_val, LV_ALIGN_CENTER, 0, 90);
    ram_pct = lv_label_create(ram_bar);  lv_obj_set_style_text_font(ram_pct, &lv_font_montserrat_32, 0);  lv_obj_align(ram_pct, LV_ALIGN_CENTER, 0, 130);

    /* --- DISK (Összevont % labellel) --- */
    disk_drive = lv_label_create(disk_bar);
    lv_obj_set_style_text_font(disk_drive, &lv_font_montserrat_24, 0);
    lv_obj_align(disk_drive, LV_ALIGN_TOP_MID, 0, 10);

    disk_val = lv_label_create(disk_bar);
    lv_obj_set_style_text_font(disk_val, &lv_font_montserrat_24, 0);
    lv_obj_align(disk_val, LV_ALIGN_TOP_MID, 0, 40);

    /* --- DISK IO --- */
    disk_io_tag = lv_label_create(disk_bar); lv_label_set_text(disk_io_tag, "DISK IO:");
    lv_obj_set_style_text_font(disk_io_tag, &lv_font_montserrat_24, 0);
    lv_obj_align(disk_io_tag, LV_ALIGN_CENTER, 0, 50);

    disk_io_val = lv_label_create(disk_bar);
    lv_obj_set_style_text_font(disk_io_val, &lv_font_montserrat_48, 0);
    lv_obj_align(disk_io_val, LV_ALIGN_CENTER, 0, 90);

    disk_io_unit = lv_label_create(disk_bar);
    lv_obj_set_style_text_font(disk_io_unit, &lv_font_montserrat_32, 0);
    lv_obj_align(disk_io_unit, LV_ALIGN_CENTER, 0, 130);

    /* --- NET IO --- */
    net_io_tag = lv_label_create(net_bar); lv_label_set_text(net_io_tag, "NET IO:");
    lv_obj_set_style_text_font(net_io_tag, &lv_font_montserrat_24, 0);
    lv_obj_align(net_io_tag, LV_ALIGN_CENTER, 0, 50);

    net_io_val = lv_label_create(net_bar);
    lv_obj_set_style_text_font(net_io_val, &lv_font_montserrat_48, 0);
    lv_obj_align(net_io_val, LV_ALIGN_CENTER, 0, 90);

    net_io_unit = lv_label_create(net_bar);
    lv_obj_set_style_text_font(net_io_unit, &lv_font_montserrat_32, 0);
    lv_obj_align(net_io_unit, LV_ALIGN_CENTER, 0, 130);

    /* Szín és alapértékek (16 elemre csökkentve) */
    lv_obj_t* all_labels[] = {
        pc_name_tag, pc_name_val, cpu_type, cpu_val, cpu_pct, ram_size, ram_val, ram_pct,
        disk_drive, disk_val, disk_io_tag, disk_io_val, disk_io_unit,
        net_io_tag, net_io_val, net_io_unit
    };
    for (int i = 0; i < 16; i++) {
        if (all_labels[i]) {
            lv_obj_set_style_text_color(all_labels[i], lv_color_white(), 0);
            if (lv_label_get_text(all_labels[i])[0] == '\0') lv_label_set_text(all_labels[i], "...");
        }
    }

    receive_thread = CreateThread(NULL, 0, udp_receive_thread, NULL, 0, NULL);
    return s_scr_page2;
}