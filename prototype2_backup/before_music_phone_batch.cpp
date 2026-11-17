#include "infotainment_ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// LVGL 8.x UI. Call infotainment_ui_create() after display/touch/LVGL initialization.
// Coordinates are native 1024x600, matching the user's landscape graph-paper drawing.
namespace {
const uint32_t BG=0x15191B, PANEL=0x202528, EDGE=0x454B50, WHITE=0xF1F3F4;
const uint32_t YELLOW=0xFFD536, RED=0xE64B48, BLUE=0x30B9F4, GREY=0x626A70;

enum Page { NAV, RADIO, PHONE, MUSIC, TEMPERATURE };

// Prototype 2 layout dimensions (1024 x 600)
constexpr int SCREEN_W = 1024;
constexpr int SCREEN_H = 600;
constexpr int HUB_X = 150;
constexpr int HUB_Y = 35;
constexpr int HUB_W = 724;
constexpr int HUB_H = 450;
constexpr int HUB_NAV_H = 76;
enum NavState { NAV_HOME, NAV_SEARCH, NAV_PRESETS, NAV_NEARBY, NAV_ROUTES, NAV_ACTIVE, NAV_ARRIVED };
enum PhoneState { CONTACTS, CALLING, CONNECTED, KEYPAD };
enum MusicState { LIBRARY, NOW_PLAYING };
Page page=TEMPERATURE; NavState nav=NAV_HOME; PhoneState phone=CONTACTS; MusicState music=LIBRARY;
lv_obj_t *root=nullptr,*content=nullptr,*sidebar=nullptr,*climate=nullptr,*vehicle=nullptr;
lv_obj_t *page_btn[4]={},*vehicle_btn[6]={},*temp_label[2]={},*temp_marker[2]={};
lv_obj_t *seat_bar[2][3]={},*fan_bar[7]={},*vol_label=nullptr,*radio_label=nullptr,*call_label=nullptr,*music_progress=nullptr;
int temp[2]={68,72}, seat_mode[2]={0,0},seat_level[2]={0,0}, fan=3,volume=18;
int radio_band=0, radio_idx[3]={0,0,0}, selected_song=0,call_seconds=0, call_wait_ms=0;
bool vehicle_on[6]={},music_playing=false,call_muted=false;
char destination[64]="1300 Academy Rd";
const char *contacts[]={"Bill","Bob","Joel","John","Will"};
const char *songs[]={"Soul Sister","2010s Hits","Hey Ya!"};
const char *artists[]={"Train","Apple Music","Outkast"};
const int presets[3][4]={{1013,957,991,879},{990,1410,1250,720},{2,411,997,63}};
const int radio_min[3]={875,530,1},radio_max[3]={1079,1710,999},radio_step[3]={2,10,1};
int current_contact=0, music_elapsed=0;
int nav_previous=NAV_HOME, nearby_category=2, search_stage=0;
bool end_confirmation=false;
char dialed[32]="";
uint32_t pseudo_random=0xA31F82u;
int radio_hold_ms=0, radio_hold_direction=0;
bool radio_hold_repeated=false;
int call_target_ms=2000;
lv_obj_t *volume_slider=nullptr,*temperature_slider[2]={};
bool updating_shell=false;
int climate_hold_type=0;
int climate_hold_id=0;
int climate_hold_ms=0;
bool climate_hold_repeated=false;
lv_obj_t *radio_minus=nullptr,*radio_plus=nullptr;
lv_obj_t *radio_preset_btn[3]={};
lv_obj_t *radio_permanent_label=nullptr;
int radio_preset_page=0;
bool radio_dial_open=false;
char radio_digits[16]="";
int radio_error_ms=0;
lv_obj_t *radio_error_label=nullptr;
const int radio_presets10[3][10]={
 {1013,957,991,879,1035,925,1067,899,973,1051},
 {990,1410,1250,720,530,670,810,1070,1510,1650},
 {2,411,997,63,18,45,120,250,500,750}
};

lv_color_t color(uint32_t rgb){return lv_color_hex(rgb);}
void paint(lv_obj_t *o,uint32_t bg=BG,uint32_t border=EDGE,int radius=3){
 lv_obj_set_style_bg_color(o,color(bg),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
 lv_obj_set_style_border_color(o,color(border),0);lv_obj_set_style_border_width(o,1,0);
 lv_obj_set_style_radius(o,radius,0);lv_obj_set_style_pad_all(o,0,0);
}
lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t bg=PANEL){
 lv_obj_t *o=lv_obj_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);paint(o,bg);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
lv_obj_t *label(lv_obj_t *parent,const char *s,int x,int y,int w,int h,uint32_t fg=WHITE){
 lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,s);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
 lv_obj_set_style_text_color(o,color(fg),0);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 lv_obj_set_style_text_font(o, &lv_font_montserrat_14, 0);
 lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);return o;
}
using CB=lv_event_cb_t;
lv_obj_t *button(lv_obj_t *p,const char *s,int x,int y,int w,int h,CB cb,intptr_t id=0,uint32_t bg=PANEL){
 lv_obj_t *b=lv_button_create(p);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);paint(b,bg);
 lv_obj_set_style_shadow_width(b,0,0);lv_obj_set_style_bg_color(b,color(YELLOW),LV_STATE_PRESSED);
 if(cb)lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void*)id);
 lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_center(l);lv_obj_set_style_text_color(l,color(bg==YELLOW?BG:WHITE),0);
 return b;
}
// Vehicle symbols are LVGL line objects, so no icon files are required.
// Each set of points uses local coordinates inside its button.
enum VehicleSymbol { FRONT_DEF, REAR_DEF, HAZARD, RECIRC, ESC_OFF, MARKER };
static const lv_point_precise_t windshield[]={{2,39},{5,25},{9,14},{16,9},{28,7},{40,9},{47,14},{51,25},{54,39},{2,39}};
static const lv_point_precise_t rear_glass[]={
    {9,6},{47,6},{47,44},{9,44},{9,6}
};
static const lv_point_precise_t steam1[]={{17,37},{14,30},{19,24},{16,18},{19,13}};
static const lv_point_precise_t steam2[]={{28,37},{25,30},{30,24},{27,18},{30,13}};
static const lv_point_precise_t steam3[]={{39,37},{36,30},{41,24},{38,18},{41,13}};
static const lv_point_precise_t triangle[]={{28,4},{52,43},{4,43},{28,4}};
static const lv_point_precise_t warning_stem[]={{28,16},{28,30}};
static const lv_point_precise_t loop[]={{45,22},{45,13},{36,7},{22,7},{12,14},{9,26},{13,37},{24,44},{38,42}};
static const lv_point_precise_t loop_tip[]={{32,35},{39,42},{46,34}};
static const lv_point_precise_t car[]={{5,32},{11,21},{18,20},{24,13},{38,13},{45,21},{51,23},{53,34},{5,34},{5,32}};
static const lv_point_precise_t skid[]={{11,43},{20,40},{29,44},{38,40},{47,43}};
static const lv_point_precise_t slash[]={{8,48},{49,7}};
static const lv_point_precise_t lamp_l[]={{21,18},{9,18},{9,35},{21,35},{21,18}};
static const lv_point_precise_t lamp_r[]={{35,18},{47,18},{47,35},{35,35},{35,18}};
static const lv_point_precise_t ray1[]={{2,16},{8,12}};
static const lv_point_precise_t ray2[]={{2,26},{8,26}};
static const lv_point_precise_t ray3[]={{2,36},{8,40}};
static const lv_point_precise_t ray4[]={{48,12},{54,16}};
static const lv_point_precise_t ray5[]={{48,26},{54,26}};
static const lv_point_precise_t ray6[]={{48,40},{54,36}};
void icon_line(lv_obj_t *parent,const lv_point_precise_t *points,uint16_t n,int x,int y,uint32_t ink=WHITE,int width=3){
 lv_obj_t *line=lv_line_create(parent);
 lv_line_set_points(line,points,n);lv_obj_set_pos(line,x,y);
 lv_obj_set_style_line_color(line,color(ink),0);
 lv_obj_set_style_line_width(line,width,0);
 lv_obj_set_style_line_rounded(line,true,0);
 lv_obj_clear_flag(line,LV_OBJ_FLAG_CLICKABLE);
}
void vehicle_icon(lv_obj_t *parent,int kind){
 const int x=38,y=40;
 if(kind==FRONT_DEF||kind==REAR_DEF){
  if(kind==FRONT_DEF){ icon_line(parent,windshield,10,x,y); static const lv_point_precise_t bottom[]={{2,39},{54,39}}; icon_line(parent,bottom,2,x,y); } else icon_line(parent,rear_glass,5,x,y);
  icon_line(parent,steam1,5,x,y);icon_line(parent,steam2,5,x,y);icon_line(parent,steam3,5,x,y);
 }else if(kind==HAZARD){
  icon_line(parent,triangle,4,x,y,RED,4);
  icon_line(parent,warning_stem,2,x,y,RED,4);
  box(parent,x+26,y+35,4,4,RED);
 }else if(kind==RECIRC){
  icon_line(parent,loop,9,x,y);icon_line(parent,loop_tip,3,x,y);
 }else if(kind==ESC_OFF){
  icon_line(parent,car,11,x,y);icon_line(parent,skid,5,x,y);
  icon_line(parent,slash,2,x,y,RED,3);
 }else if(kind==MARKER){
  icon_line(parent,lamp_l,5,x,y);icon_line(parent,lamp_r,5,x,y);
  icon_line(parent,ray1,2,x,y);icon_line(parent,ray2,2,x,y);
  icon_line(parent,ray3,2,x,y);icon_line(parent,ray4,2,x,y);
  icon_line(parent,ray5,2,x,y);icon_line(parent,ray6,2,x,y);
 }
}
lv_obj_t *vehicle_indicator[6]={};

void clear(lv_obj_t *o){lv_obj_clean(o);}
void render_content();void refresh_shell();void temperature_draw();
void navigate(NavState next){nav_previous=(int)nav;if(next==NAV_SEARCH)search_stage=0;nav=next;render_content();}
void set_destination(const char *s){snprintf(destination,sizeof(destination),"%s",s);navigate(NAV_ROUTES);}
void page_click(lv_event_t *e){
 radio_hold_direction=0;
 radio_hold_ms=0;

 Page next=(Page)(intptr_t)lv_event_get_user_data(e);

 // Switching to Radio pauses music.
 if(next==RADIO){
  music_playing=false;
 }

 page=next;

 if(page==NAV){
  nav=NAV_HOME;
 }

 render_content();
 refresh_shell();
}
void update_navigation_buttons(){
 const char *names[]={"Navigation","Radio","Phone","Music"};

 for(int i=0;i<4;i++){
  if(!page_btn[i])continue;

  bool active=(page!=TEMPERATURE && page==(Page)i);

  lv_obj_t *text=lv_obj_get_child(page_btn[i],0);

  lv_label_set_text(
   text,
   active?"Temperature":names[i]
  );

  lv_obj_remove_event_cb(page_btn[i],page_click);

  lv_obj_add_event_cb(
   page_btn[i],
   page_click,
   LV_EVENT_CLICKED,
   (void*)(intptr_t)(active?TEMPERATURE:i)
  );

  lv_obj_set_style_bg_color(
   page_btn[i],color(PANEL),0
  );

  lv_obj_set_style_text_color(
   text,color(WHITE),0
  );
 }
}
void vehicle_click(lv_event_t *e){int n=(int)(intptr_t)lv_event_get_user_data(e);vehicle_on[n]=!vehicle_on[n];refresh_shell();}
void temperature_click(lv_event_t *e){int n=(int)(intptr_t)lv_event_get_user_data(e);int side=n/2,delta=n%2?1:-1;temp[side]+=delta;if(temp[side]<59)temp[side]=59;if(temp[side]>86)temp[side]=86;refresh_shell();}
void temperature_drag(lv_event_t *e){if(updating_shell)return;int side=(int)(intptr_t)lv_event_get_user_data(e);temp[side]=lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));refresh_shell();}
void volume_drag(lv_event_t *e){if(updating_shell)return;volume=lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));refresh_shell();}
void seat_click(lv_event_t *e){int n=(int)(intptr_t)lv_event_get_user_data(e),side=n/2,mode=n%2+1;
 if(seat_mode[side]!=mode){seat_mode[side]=mode;seat_level[side]=1;}
 else {seat_level[side]=(seat_level[side]+1)%4;if(!seat_level[side])seat_mode[side]=0;}
 refresh_shell();}
void fan_click(lv_event_t *e){fan+=(intptr_t)lv_event_get_user_data(e)?1:-1;if(fan<1)fan=1;if(fan>7)fan=7;refresh_shell();}
void volume_click(lv_event_t *e){volume+=(intptr_t)lv_event_get_user_data(e)?1:-1;if(volume<0)volume=0;if(volume>40)volume=40;refresh_shell();}
void climate_hold_event(lv_event_t *e){
 lv_event_code_t event=lv_event_get_code(e);
 int id=(int)(intptr_t)lv_event_get_user_data(e);
 if(event==LV_EVENT_PRESSED){
  climate_hold_type=id<4?1:(id<6?2:3);
  climate_hold_id=id<4?id:(id<6?id-4:id-6);
  climate_hold_ms=0;
  climate_hold_repeated=false;
 }else if(event==LV_EVENT_RELEASED||event==LV_EVENT_PRESS_LOST){
  climate_hold_type=0;
  climate_hold_ms=0;
 }
}

static void slider_extended_touch(lv_event_t *e){
 lv_event_code_t event=lv_event_get_code(e);
 if(event!=LV_EVENT_PRESSED && event!=LV_EVENT_PRESSING) return;

 lv_obj_t *slider=(lv_obj_t*)lv_event_get_user_data(e);
 lv_indev_t *indev=lv_indev_active();
 if(!indev || !slider) return;

 lv_point_t point;
 lv_indev_get_point(indev,&point);

 lv_area_t area;
 lv_obj_get_coords(slider,&area);

 int width=area.x2-area.x1+1;
 if(width<=1) return;

 int minimum=lv_slider_get_min_value(slider);
 int maximum=lv_slider_get_max_value(slider);

 int position=point.x-area.x1;
 if(position<0) position=0;
 if(position>width-1) position=width-1;

 int value=minimum+(position*(maximum-minimum)+(width-1)/2)/(width-1);

 if(lv_slider_get_value(slider)!=value){
  lv_slider_set_value(slider,value,LV_ANIM_OFF);
  lv_obj_send_event(slider,LV_EVENT_VALUE_CHANGED,nullptr);
 }
}
void draw_climate(){
 clear(climate);box(climate,0,0,1024,112,BG);
 for(int side=0;side<2;side++){
  int x=side?832:0;box(climate,x,2,192,108);
  lv_obj_t *tm=button(climate,"-",x+9,13,37,45,temperature_click,side*2); lv_obj_add_event_cb(tm,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)(side*2)); lv_obj_set_ext_click_area(tm,9);
  temp_label[side]=label(climate,"",x+51,21,87,40); lv_obj_set_style_text_font(temp_label[side],&lv_font_montserrat_28,0);
  lv_obj_t *tp=button(climate,"+",x+146,13,37,45,temperature_click,side*2+1); lv_obj_add_event_cb(tp,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)(side*2+1)); lv_obj_set_ext_click_area(tp,9);
  temperature_slider[side]=lv_slider_create(climate);
  lv_obj_set_pos(temperature_slider[side],x+18,77);lv_obj_set_size(temperature_slider[side],155,12); 
  lv_slider_set_range(temperature_slider[side],59,86);
  lv_obj_set_style_bg_color(temperature_slider[side],color(GREY),LV_PART_MAIN); lv_obj_set_style_bg_opa(temperature_slider[side],LV_OPA_COVER,LV_PART_MAIN);
  lv_obj_set_style_bg_color(temperature_slider[side],color(RED),LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(temperature_slider[side],color(WHITE),LV_PART_KNOB);
  lv_obj_add_event_cb(temperature_slider[side],temperature_drag,LV_EVENT_VALUE_CHANGED,(void*)(intptr_t)side);
  lv_obj_t *temp_extra=lv_obj_create(climate);
  lv_obj_set_pos(temp_extra,x+18,89);
  lv_obj_set_size(temp_extra,155,23);
  lv_obj_set_style_bg_opa(temp_extra,LV_OPA_TRANSP,0);
  lv_obj_set_style_border_width(temp_extra,0,0);
  lv_obj_set_style_pad_all(temp_extra,0,0);
  lv_obj_clear_flag(temp_extra,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(temp_extra,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(temp_extra,slider_extended_touch,LV_EVENT_ALL,temperature_slider[side]);
  temp_marker[side]=nullptr;
 }
 
 for(int side=0;side<2;side++){
  int x=side?662:196;box(climate,x,2,166,108);
  label(climate,"SEAT",x+4,6,158,22);
  button(climate,LV_SYMBOL_CHARGE,x+5,33,48,53,seat_click,side*2);
  button(climate,LV_SYMBOL_DOWN,x+113,33,48,53,seat_click,side*2+1);
  for(int i=0;i<3;i++)seat_bar[side][i]=box(climate,x+65,37+i*17,36,10,GREY);
 }
 box(climate,366,2,292,108);

 // FAN - upper half
 label(climate,"FAN",473,4,78,20);
 lv_obj_t *fm=button(climate,"-",374,25,42,31,fan_click,0); lv_obj_add_event_cb(fm,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)6);
 for(int i=0;i<7;i++){
  fan_bar[i]=box(climate,438+i*20,43-i*3,14,12+i*3,GREY);
 }
 lv_obj_t *fp=button(climate,"+",608,25,42,31,fan_click,1); lv_obj_add_event_cb(fp,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)7);

 // VOLUME - lower half
 label(climate,"VOLUME",465,61,94,20);
 lv_obj_t *vm=button(climate,"-",374,76,42,30,volume_click,0);
 lv_obj_add_event_cb(vm,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)4); lv_obj_set_ext_click_area(vm,4);

 volume_slider=lv_slider_create(climate);
 lv_obj_set_pos(volume_slider,434,84);
 lv_obj_set_size(volume_slider,130,17);
 lv_slider_set_range(volume_slider,0,40);
 lv_obj_set_style_bg_color(volume_slider,color(GREY),LV_PART_MAIN);
 lv_obj_set_style_bg_color(volume_slider,color(YELLOW),LV_PART_INDICATOR);
 lv_obj_set_style_bg_color(volume_slider,color(WHITE),LV_PART_KNOB);
 lv_obj_set_style_pad_all(volume_slider,3,LV_PART_KNOB);
 lv_obj_add_event_cb(volume_slider,volume_drag,LV_EVENT_VALUE_CHANGED,nullptr);

 lv_obj_t *volume_extra=lv_obj_create(climate);
 lv_obj_set_pos(volume_extra,420,52);
 lv_obj_set_size(volume_extra,150,60);
 lv_obj_set_style_bg_opa(volume_extra,LV_OPA_TRANSP,0);
 lv_obj_set_style_border_width(volume_extra,0,0);
 lv_obj_set_style_pad_all(volume_extra,0,0);
 lv_obj_clear_flag(volume_extra,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_flag(volume_extra,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_add_event_cb(volume_extra,slider_extended_touch,LV_EVENT_ALL,volume_slider);

 vol_label=label(climate,"",575,80,33,25); lv_obj_set_style_text_font(vol_label,&lv_font_montserrat_20,0);

 lv_obj_t *vp=button(climate,"+",608,76,42,30,volume_click,1);
 lv_obj_add_event_cb(vp,climate_hold_event,LV_EVENT_ALL,(void*)(intptr_t)5); lv_obj_set_ext_click_area(vp,4);
}
void refresh_shell(){
 updating_shell=true;
 update_navigation_buttons();
 for(int i=0;i<6;i++){
  lv_obj_set_style_bg_color(vehicle_btn[i],color(PANEL),0);
  lv_obj_set_style_bg_color(vehicle_indicator[i],color(vehicle_on[i]?YELLOW:GREY),0);
 }
 for(int s=0;s<2;s++){
  char b[16];if(temp[s]==59)strcpy(b,"LO");else if(temp[s]==86)strcpy(b,"HI");else snprintf(b,sizeof(b),"%d\xC2\xB0",temp[s]);
  lv_label_set_text(temp_label[s],b);
  if(temperature_slider[s])lv_slider_set_value(temperature_slider[s],temp[s],LV_ANIM_OFF);
  for(int i=0;i<3;i++){
   uint32_t c=i>=3-seat_level[s]?(seat_mode[s]==1?RED:BLUE):GREY;
   if(seat_bar[s][i])lv_obj_set_style_bg_color(seat_bar[s][i],color(c),0);
  }
 }
 for(int i=0;i<7;i++)if(fan_bar[i])lv_obj_set_style_bg_color(fan_bar[i],color(i<fan?YELLOW:GREY),0);
 char b[12];snprintf(b,sizeof(b),"%d",volume);lv_label_set_text(vol_label,b);if(volume_slider)lv_slider_set_value(volume_slider,volume,LV_ANIM_OFF);
 updating_shell=false;
}
void nav_click(lv_event_t *e){
 int action=(int)(intptr_t)lv_event_get_user_data(e);
 if(action>=0&&action<=4){if(action>=2)nearby_category=action;navigate((NavState)(action==0?NAV_SEARCH:action==1?NAV_PRESETS:NAV_NEARBY));}
 else if(action==5){NavState target=(NavState)nav_previous; if(nav==NAV_ROUTES && target==NAV_SEARCH)search_stage=0; if(target==nav)target=NAV_HOME; nav=target;nav_previous=NAV_HOME;end_confirmation=false;render_content();}
 else if(action==6){set_destination("123 Sesame St");}
 else if(action==7){navigate(NAV_ACTIVE);}
 else if(action==8){end_confirmation=true;render_content();}
 else if(action==9){nav=NAV_HOME;nav_previous=NAV_HOME;search_stage=0;end_confirmation=false;render_content();}
 else if(action==10){end_confirmation=false;render_content();}
 else if(action==11){search_stage=1;render_content();}
}
void preset_click(lv_event_t *e){int i=(int)(intptr_t)lv_event_get_user_data(e);set_destination(i==3?"1951 Crescent Ln":"1300 Academy Rd");}
void nearby_click(lv_event_t *e){(void)e;set_destination("123 Sesame St");}
void nav_draw(){
 if(nav==NAV_HOME){
  const char *opts[]={"Search","Presets","Food","Gas","Hotel"};
  for(int i=0;i<5;i++)button(content,opts[i],10,10+i*70,158,62,nav_click,i,PANEL);
  box(content,178,10,320,354,0x1A2B3C);
  // Stylized map placeholder, not a live mapping service.
  for(int i=0;i<5;i++)box(content,188+i*60,22,2,328,0x36536C);
  for(int i=0;i<4;i++)box(content,188,54+i*81,300,2,0x36536C);
  label(content,"N",307,110,60,30,YELLOW);
  label(content,"Ã¢â€”â€ ",290,165,95,90,YELLOW);
  return;
 }
 if(nav!=NAV_ACTIVE && nav!=NAV_ARRIVED) button(content,"< Back",12,10,95,45,nav_click,5);
 if(nav==NAV_SEARCH){label(content,"Search destination (demo)",115,32,380,42);
  button(content,"Microphone / Simulate",145,135,280,75,nav_click,11);
  label(content,search_stage?"Heard: 123 Sesame St":"Tap microphone to simulate",125,230,350,40);
  if(search_stage)button(content,"Select destination",145,290,280,55,nav_click,6);
 }
 if(nav==NAV_PRESETS){const char *a[]={"Home","Work","Daycare","1951 Crescent..."};for(int i=0;i<4;i++)button(content,a[i],130,35+i*82,345,66,preset_click,i);}
 if(nav==NAV_NEARBY){label(content,nearby_category==2?"Food results (demo)":nearby_category==3?"Gas results (demo)":"Hotel results (demo)",115,65,385,35);for(int i=0;i<3;i++){char b[32];snprintf(b,sizeof(b),"Result %d",i+1);button(content,b,135,110+i*80,340,65,nearby_click);}}
 if(nav==NAV_ROUTES){label(content,destination,120,55,380,40,YELLOW);const char *a[]={"21 min  GO","17 min  GO","19 min  GO"};for(int i=0;i<3;i++)button(content,a[i],130,110+i*80,340,65,nav_click,7);}
 if(nav==NAV_ACTIVE||nav==NAV_ARRIVED){box(content,130,65,355,190,0x1A2B3C);label(content,nav==NAV_ARRIVED?"ARRIVED":"ACTIVE NAVIGATION",150,90,315,50,YELLOW);label(content,"ETA   Ã¢â‚¬Â¢   MIN   Ã¢â‚¬Â¢   MI",160,190,300,40);button(content,"END",247,280,120,58,nav_click,8);
  if(end_confirmation){box(content,90,95,400,220,0x292E32);label(content,"End navigation?",130,123,320,45,YELLOW);
   button(content,"Cancel",112,223,165,60,nav_click,10);
   button(content,"End",303,223,165,60,nav_click,9,RED);}
 }
}
void radio_frequency_text(char *out,size_t size){
 int v=radio_idx[radio_band];
 if(v==0)v=radio_min[radio_band];
 if(radio_band==0)
  snprintf(out,size,"%d.%d MHz",v/10,v%10);
 else if(radio_band==1)
  snprintf(out,size,"%d kHz",v);
 else
  snprintf(out,size,"XM %d",v);
}

void radio_refresh(){
 char b[40];
 radio_frequency_text(b,sizeof(b));

 if(radio_label)lv_label_set_text(radio_label,b);
 if(radio_permanent_label)lv_label_set_text(radio_permanent_label,b);

 if(page!=RADIO || radio_dial_open)return;

 for(int i=0;i<3;i++){
  lv_obj_t *btn=radio_preset_btn[i];
  if(!btn)continue;

  int index=radio_preset_page*3+i;
  if(index>=10)continue;

  bool selected=radio_idx[radio_band]==radio_presets10[radio_band][index];

  lv_obj_set_style_bg_color(
   btn,color(selected?YELLOW:PANEL),0
  );

  lv_obj_t *txt=lv_obj_get_child(btn,0);
  if(txt)lv_obj_set_style_text_color(
   txt,color(selected?BG:WHITE),0
  );
 }
}

void tune_step(int direction){
 int &v=radio_idx[radio_band];
 if(v==0)v=radio_min[radio_band];

 v+=direction*radio_step[radio_band];

 if(v>radio_max[radio_band])v=radio_min[radio_band];
 if(v<radio_min[radio_band])v=radio_max[radio_band];

 radio_refresh();
}

void tune_click(lv_event_t *e){
 if(radio_hold_repeated)return;
 int direction=(intptr_t)lv_event_get_user_data(e)?1:-1;
 tune_step(direction);
}

void tune_press(lv_event_t *e){
 lv_event_code_t c=lv_event_get_code(e);

 if(c==LV_EVENT_PRESSED){
  radio_hold_ms=0;
  radio_hold_repeated=false;
  radio_hold_direction=(intptr_t)lv_event_get_user_data(e)?1:-1;
 }else if(c==LV_EVENT_RELEASED || c==LV_EVENT_PRESS_LOST){
  radio_hold_direction=0;
  radio_hold_ms=0;
 }
}

void band_click(lv_event_t *e){
 radio_band=(int)(intptr_t)lv_event_get_user_data(e);
 if(radio_idx[radio_band]==0)
  radio_idx[radio_band]=radio_min[radio_band];
 radio_preset_page=0;
 radio_dial_open=false;
 render_content();
}

void radio_preset(lv_event_t *e){
 int index=(int)(intptr_t)lv_event_get_user_data(e);
 if(index<0 || index>=10)return;

 radio_idx[radio_band]=radio_presets10[radio_band][index];
 render_content();
 radio_refresh();
}

void radio_page_click(lv_event_t *e){
 int direction=(int)(intptr_t)lv_event_get_user_data(e);
 radio_preset_page+=direction;

 if(radio_preset_page<0)radio_preset_page=3;
 if(radio_preset_page>3)radio_preset_page=0;

 render_content();
}

void radio_dial_open_click(lv_event_t *){
 radio_dial_open=true;
 radio_digits[0]=0;
 radio_error_ms=0;
 render_content();
}

void radio_dial_back(lv_event_t *){
 radio_dial_open=false;
 radio_error_ms=0;
 radio_digits[0]=0;
 render_content();
}

void radio_dial_key(lv_event_t *e){
 int key=(int)(intptr_t)lv_event_get_user_data(e);
 size_t len=strlen(radio_digits);

 if(key>=0 && key<=9){
  if(len<sizeof(radio_digits)-1){
   radio_digits[len]=(char)('0'+key);
   radio_digits[len+1]=0;
  }
 }else if(key==10){
  // Clear
  radio_digits[0]=0;
 }else if(key==11){
  // Backspace
  if(len>0)radio_digits[len-1]=0;
 }else if(key==12){
  // Enter
  int entered=atoi(radio_digits);
  int frequency=entered;

  // FM: 1013 represents 101.3 MHz.
  // Also accept 3 digits such as 995 for 99.5.
  bool valid=strlen(radio_digits)>0;

  if(radio_band==0){
   valid=valid && strlen(radio_digits)>=3 &&
         frequency>=radio_min[0] &&
         frequency<=radio_max[0] &&
         ((frequency-radio_min[0])%2==0);
  }else if(radio_band==1){
   valid=valid &&
         frequency>=radio_min[1] &&
         frequency<=radio_max[1] &&
         ((frequency-radio_min[1])%10==0);
  }else{
   valid=valid &&
         frequency>=radio_min[2] &&
         frequency<=radio_max[2];
  }

  if(valid){
   radio_idx[radio_band]=frequency;
   radio_dial_open=false;
   radio_error_ms=0;
   render_content();
   radio_refresh();
   return;
  }

  radio_error_ms=1000;
 }

 render_content();
}

void radio_dial_draw(){
 label(content,"Radio Dial",15,8,210,30,YELLOW);

 char current[40];
 radio_frequency_text(current,sizeof(current));
 label(content,current,245,8,350,30);

 label(content,radio_digits,15,43,585,35,YELLOW);

 const char *keys[]={
  "1","2","3","4","5",
  "6","7","8","9","0"
 };

 // Five columns, two rows
 for(int i=0;i<10;i++){
  int x=15+(i%5)*121;
  int y=88+(i/5)*83;

  button(content,keys[i],x,y,112,73,
         radio_dial_key,i);
 }

 button(content,"< Back",15,265,105,55,radio_dial_back);
 button(content,"DEL",130,265,100,55,radio_dial_key,11);
 button(content,"CLR",240,265,100,55,radio_dial_key,10);
 button(content,"ENTER",350,265,260,55,radio_dial_key,12,YELLOW);

 radio_error_label=nullptr;

 if(radio_error_ms>0){
  radio_error_label=label(
   content,"Invalid Frequency",160,330,320,30,RED
  );
 }
}

void radio_draw(){
 radio_label=nullptr;
 radio_error_label=nullptr;

 for(int i=0;i<3;i++)radio_preset_btn[i]=nullptr;

 if(radio_idx[radio_band]==0)
  radio_idx[radio_band]=radio_min[radio_band];

 if(radio_dial_open){
  radio_dial_draw();
  return;
 }

 const char *bands[]={"FM","AM","SiriusXM"};

 for(int i=0;i<3;i++){
  button(content,bands[i],
         12+i*207,8,197,48,
         band_click,i,
         i==radio_band?YELLOW:PANEL);
 }

 char frequency[40];
 radio_frequency_text(frequency,sizeof(frequency));

 radio_minus=button(content,"<",15,82,85,75,tune_click,0);
 lv_obj_add_event_cb(
  radio_minus,tune_press,LV_EVENT_ALL,(void*)0
 );

 radio_label=label(
  content,frequency,115,95,395,52,YELLOW
 );
 lv_obj_set_style_text_font(
  radio_label,&lv_font_montserrat_28,0
 );

 radio_plus=button(content,">",525,82,85,75,tune_click,1);
 lv_obj_add_event_cb(
  radio_plus,tune_press,LV_EVENT_ALL,(void*)1
 );

 button(content,"DIAL",245,171,140,43,
        radio_dial_open_click);

 for(int i=0;i<3;i++){
  int index=radio_preset_page*3+i;
  if(index>=10)continue;

  int v=radio_presets10[radio_band][index];

  char text[32];
  if(radio_band==0)
   snprintf(text,sizeof(text),"%d.%d",v/10,v%10);
  else
   snprintf(text,sizeof(text),"%d",v);

  bool selected=radio_idx[radio_band]==v;

  radio_preset_btn[i]=button(
   content,text,
   15+i*207,230,197,65,
   radio_preset,index,
   selected?YELLOW:PANEL
  );
 }

 button(content,"<",15,311,65,46,radio_page_click,-1);

 char page_text[40];
 snprintf(page_text,sizeof(page_text),
          "Presets %d / 4",radio_preset_page+1);
 label(content,page_text,210,317,210,35);

 button(content,">",545,311,65,46,radio_page_click,1);
}
void contact_click(lv_event_t *e){current_contact=(int)(intptr_t)lv_event_get_user_data(e);phone=CALLING;call_wait_ms=0;call_seconds=0;call_muted=false;dialed[0]=0;
pseudo_random=pseudo_random*1664525u+1013904223u;call_target_ms=1500+(int)(pseudo_random%3501u);render_content();}
void phone_action(lv_event_t *e){int a=(int)(intptr_t)lv_event_get_user_data(e);if(a==0){phone=CONTACTS;call_seconds=0;}else if(a==1){phone=KEYPAD;}else if(a==2){call_muted=!call_muted;}else if(a==3){phone=CONNECTED;}render_content();}
void dial_click(lv_event_t *e){
 const char *keys="123456789*0#";int index=(int)(intptr_t)lv_event_get_user_data(e);
 size_t n=strlen(dialed);if(n<sizeof(dialed)-2){dialed[n]=keys[index];dialed[n+1]=0;}render_content();
}
void phone_draw(){
 if(phone==CONTACTS){label(content,"Contacts",15,14,155,36);for(int i=0;i<8;i++){
  char b[40];snprintf(b,sizeof(b),"%s",i<5?contacts[i]:(i==7?"ADD":"+"));
  button(content,b,14+(i%4)*128,80+(i/4)*135,116,118,i<5?contact_click:nullptr,i);
 }return;}
 char b[90];snprintf(b,sizeof(b),"%s %s",contacts[current_contact],phone==CALLING?"Calling...":"Connected");
 label(content,b,70,40,425,55,YELLOW);
 if(phone==CONNECTED||phone==KEYPAD){char t[32];snprintf(t,sizeof(t),"%d:%02d",call_seconds/60,call_seconds%60);call_label=label(content,t,190,95,170,38);}
 if(phone==KEYPAD){label(content,dialed,155,124,250,25,YELLOW);const char *keys[]={"1","2","3","4","5","6","7","8","9","*","0","#"};for(int i=0;i<12;i++)button(content,keys[i],160+(i%3)*80,150+(i/3)*37,65,33,dial_click,i);}
 else if(phone==CONNECTED){button(content,call_muted?"MUTE ON":"MUTE",110,195,130,60,phone_action,2);button(content,"DIAL",285,195,130,60,phone_action,1);}
 if(phone==KEYPAD)button(content,"< Call",15,300,115,50,phone_action,3);
 button(content,"END",190,300,180,52,phone_action,0,RED);
}
void music_click(lv_event_t *e){selected_song=(int)(intptr_t)lv_event_get_user_data(e);music=NOW_PLAYING;music_playing=true;music_elapsed=0;render_content();}
void music_action(lv_event_t *e){int a=(int)(intptr_t)lv_event_get_user_data(e);if(a==0)music=LIBRARY;else music_playing=!music_playing;render_content();}
void music_draw(){
 if(music==LIBRARY){for(int i=0;i<3;i++){char b[100];snprintf(b,sizeof(b),"%s  -  %s",songs[i],artists[i]);button(content,b,15,15+i*90,512,77,music_click,i);}button(content,"+ Add",15,286,512,67,nullptr);return;}
 button(content,"< Library",15,12,100,42,music_action,0);label(content,songs[selected_song],100,65,360,55,YELLOW);
 label(content,artists[selected_song],120,120,320,35);
 box(content,90,195,380,14,GREY);music_progress=box(content,90,195,5+375*music_elapsed/210,14,YELLOW);
 char progress_text[40];snprintf(progress_text,sizeof(progress_text),"%d:%02d / 3:30",music_elapsed/60,music_elapsed%60);
 label(content,progress_text,205,215,160,26);
 button(content,music_playing?"PAUSE":"PLAY",200,250,160,76,music_action,1);
}
void temperature_draw(){
 // Prototype 2 initial screen
 // Four large sections in the central hub.
 const char *names[]={"NAVIGATION","RADIO","MUSIC","PHONE"};
 const int pages[]={NAV,RADIO,MUSIC,PHONE};

 for(int i=0;i<4;i++){
  int col=i%2;
  int row=i/2;

  int x=8+col*358;
  int y=8+row*221;

  lv_obj_t *b=button(
   content,names[i],
   x,y,350,213,
   page_click,pages[i]
  );

  lv_obj_set_style_text_font(
   lv_obj_get_child(b,0),
   &lv_font_montserrat_20,0
  );
 }
}
void render_content(){if(page==TEMPERATURE){
 lv_obj_add_flag(sidebar,LV_OBJ_FLAG_HIDDEN);
 lv_obj_set_pos(content,HUB_X,HUB_Y);
 lv_obj_set_size(content,HUB_W,HUB_H);
}else{
 lv_obj_clear_flag(sidebar,LV_OBJ_FLAG_HIDDEN);
 lv_obj_set_pos(content,HUB_X,HUB_Y+HUB_NAV_H);
 lv_obj_set_size(content,HUB_W,HUB_H-HUB_NAV_H);
}radio_hold_direction=0;call_label=nullptr;music_progress=nullptr;radio_minus=nullptr;radio_plus=nullptr;radio_label=nullptr;radio_error_label=nullptr;for(int i=0;i<3;i++)radio_preset_btn[i]=nullptr;clear(content);if(page==NAV)nav_draw();else if(page==RADIO)radio_draw();else if(page==PHONE)phone_draw();else if(page==MUSIC)music_draw();else if(page==TEMPERATURE)temperature_draw();}
void tick(lv_timer_t *){
 static int elapsed_ms=0;
 if(climate_hold_type!=0){
  climate_hold_ms+=50;
  int delay=400;
  int interval=climate_hold_type==1?150:(climate_hold_type==2?100:250);
  if(climate_hold_ms>=delay && (climate_hold_ms-delay)%interval==0){
   climate_hold_repeated=true;
   int direction=(climate_hold_id%2)?1:-1;
   if(climate_hold_type==3){fan+=direction;if(fan<1)fan=1;if(fan>7)fan=7;}else if(climate_hold_type==1){
    int side=climate_hold_id/2;
    temp[side]+=direction;
    if(temp[side]<59)temp[side]=59;
    if(temp[side]>86)temp[side]=86;
   }else{
    volume+=direction;
    if(volume<0)volume=0;
    if(volume>40)volume=40;
   }
   refresh_shell();
  }
 }
 if(page==RADIO&&radio_hold_direction!=0){
  radio_hold_ms+=50;
  if(radio_hold_ms>=400 && (radio_hold_ms-400)%100==0){
   radio_hold_repeated=true;tune_step(radio_hold_direction);
  }
 }
 if(radio_error_ms>0){
 radio_error_ms-=50;
 if(radio_error_ms<=0){
  radio_error_ms=0;
  if(page==RADIO && radio_dial_open)render_content();
 }
}
if(phone==CALLING){call_wait_ms+=50;if(call_wait_ms>=call_target_ms){phone=CONNECTED;if(page==PHONE)render_content();}}
 elapsed_ms+=50;if(elapsed_ms<1000)return;elapsed_ms=0;
 if(phone==CONNECTED||phone==KEYPAD){call_seconds++;if(page==PHONE&&call_label){char b[24];snprintf(b,sizeof(b),"%d:%02d",call_seconds/60,call_seconds%60);lv_label_set_text(call_label,b);}}
 if(music_playing){music_elapsed++;if(music_elapsed>=210)music_elapsed=0;if(page==MUSIC&&music==NOW_PLAYING&&music_progress)lv_obj_set_width(music_progress,5+375*music_elapsed/210);}
}
} // namespace
extern "C" void infotainment_ui_create(void){
 root=lv_screen_active();lv_obj_set_style_bg_color(root,color(BG),0);lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_size(root,1024,600);clear(root);
 box(root,0,0,1024,35,BG);label(root,"72\xC2\xB0" "F",8,8,75,23);label(root,"6:24 PM",920,8,90,23);
 // Top: dynamic content 0..550, permanent main buttons 550..740, vehicle buttons 740..1024.
 content=box(root,HUB_X,HUB_Y+HUB_NAV_H,HUB_W,HUB_H-HUB_NAV_H,BG);
 sidebar=box(root,HUB_X,HUB_Y,HUB_W,HUB_NAV_H,BG);
 const char *pages[]={"NAV","RADIO","PHONE","MUSIC"};
 // Visual order top-to-bottom from the user's rotated drawing: Music, Phone, Radio, Navigation.
 const int page_order[]={NAV,MUSIC,RADIO,PHONE};
for(int i=0;i<4;i++){
 int p=page_order[i];
 page_btn[p]=button(sidebar,pages[p],5+i*179,5,174,66,page_click,p);
 lv_obj_set_style_text_font(
  lv_obj_get_child(page_btn[p],0),
  &lv_font_montserrat_20,0
 );
}
 vehicle=root;
 for(int i=0;i<6;i++){
  int vx=(i<3)?8:884;
int vy=45+(i%3)*147;
vehicle_btn[i]=button(vehicle,"",vx,vy,132,138,vehicle_click,i);
  vehicle_indicator[i]=box(vehicle_btn[i],18,7,96,5,GREY);
  vehicle_icon(vehicle_btn[i],i);
 }
 climate=box(root,0,485,1024,115,BG);
 draw_climate();
 render_content();
 refresh_shell();
  lv_timer_create(tick,50,nullptr);
}










































