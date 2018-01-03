/* TFT demo

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <time.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "bt.h"

#include "freertos/queue.h"
#include "soc/timer_group_struct.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"

#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_types.h"
#include "tftspi.h"
#include "tft.h"
#include "spiffs_vfs.h"
#include "slidebutton.h"
#include "bluetooth.h"

#define TEST_DEVICE_NAME            "ESP_LED_DRIVER_DEMO"
#define TEST_MANUFACTURER_DATA_LEN  17

#define PREPARE_BUF_MAX_SIZE 1024




/*-------------------------------------------------------------*/
/* ESPDisplay setup, and setting defined variable and constants. */

int is_display_time = 1;
int is_display_date = 1;
char* display_time_format = "HH:MM:SS";
char* display_date_format = "YYYY:MM:DD";


#define TIME_X_COORD	248

/*-------------------------------------------------------------*/

#ifdef CONFIG_EXAMPLE_USE_WIFI

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include <sys/time.h>
#include <unistd.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include "esp_log.h"

#endif

/*
 * A sample structure to pass events
 * from the timer interrupt handler to the main program.
 */
typedef struct {
    int type;  // the type of timer's event
    int timer_group;
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

static s_slide slide01;
static s_slide slide02;
static s_slide slide03;

xQueueHandle timer_queue;
static void _dispTime();

#define TIMER_DIVIDER         16  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define TIMER_INTERVAL0_SEC   (1) // sample test interval for the first timer
#define TIMER_INTERVAL1_SEC   (5.78)   // sample test interval for the second timer
#define TEST_WITHOUT_RELOAD   0        // testing will be done without auto reload
#define TEST_WITH_RELOAD      1        // testing will be done with auto reload

// ==================================================
// Define which spi bus to use VSPI_HOST or HSPI_HOST
#define SPI_BUS HSPI_HOST
// ==================================================


static int _demo_pass = 0;
static uint8_t doprint = 1;
static uint8_t run_gs_demo = 0; // Run gray scale demo if set to 1
/* Last tm for display time */
static struct tm* tm_info;
static char tmp_buff[64];

static int last_hour = -1;
static int last_min = -1;
static int last_sec = -1;

static time_t time_now, time_last = 0;
//static const char *file_fonts[3] = {"/spiffs/fonts/DotMatrix_M.fon", "/spiffs/fonts/Ubuntu.fon", "/spiffs/fonts/Grotesk24x48.fon"};
static uint8_t disp_rot = PORTRAIT;

static void print_display();

#define GDEMO_TIME 1000
#define GDEMO_INFO_TIME 5000


//==================================================================================
#ifdef CONFIG_EXAMPLE_USE_WIFI

static const char tag[] = "[TFT Demo]";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = 0x00000001;


//------------------------------------------------------------
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

//-------------------------------
static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_LOGI(tag, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

//-------------------------------
static void initialize_sntp(void)
{
    ESP_LOGI(tag, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

//--------------------------
static int obtain_time(void)
{
	int res = 1;
    initialise_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = 20;

    time(&time_now);
	tm_info = localtime(&time_now);

    while(tm_info->tm_year < (2016 - 1900) && ++retry < retry_count) {
        //ESP_LOGI(tag, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		sprintf(tmp_buff, "Wait %0d/%d", retry, retry_count);
    	TFT_print(tmp_buff, CENTER, LASTY);
		vTaskDelay(500 / portTICK_RATE_MS);
        time(&time_now);
    	tm_info = localtime(&time_now);
    }
    if (tm_info->tm_year < (2016 - 1900)) {
    	ESP_LOGI(tag, "System time NOT set.");
    	res = 0;
    }
    else {
    	ESP_LOGI(tag, "System time is set.");
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
    return res;
}

#endif  //CONFIG_EXAMPLE_USE_WIFI
//==================================================================================

static int sec_flag = 0;

/*
 * Timer group0 ISR handler
 *
 * Note:
 * We don't call the timer API here because they are not declared with IRAM_ATTR.
 * If we're okay with the timer irq not being serviced while SPI flash cache is disabled,
 * we can allocate this interrupt without the ESP_INTR_FLAG_IRAM flag and use the normal API.
 */
void IRAM_ATTR timer_group0_isr(void *para)
{
    int timer_idx = (int) para;

    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t intr_status = TIMERG0.int_st_timers.val;
    TIMERG0.hw_timer[timer_idx].update = 1;
    uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[timer_idx].cnt_high) << 32
        | TIMERG0.hw_timer[timer_idx].cnt_low;

    /* Prepare basic event data
       that will be then sent back to the main program task */
    timer_event_t evt;
    evt.timer_group = 0;
    evt.timer_idx = timer_idx;
    evt.timer_counter_value = timer_counter_value;

    /* Clear the interrupt
       and update the alarm time for the timer with without reload */
    if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_0) {
        evt.type = TEST_WITHOUT_RELOAD;
        TIMERG0.int_clr_timers.t0 = 1;
        timer_counter_value += (uint64_t) (TIMER_INTERVAL0_SEC * TIMER_SCALE);
        TIMERG0.hw_timer[timer_idx].alarm_high = (uint32_t) (timer_counter_value >> 32);
        TIMERG0.hw_timer[timer_idx].alarm_low = (uint32_t) timer_counter_value;
    } else if ((intr_status & BIT(timer_idx)) && timer_idx == TIMER_1) {
        evt.type = TEST_WITH_RELOAD;
        TIMERG0.int_clr_timers.t1 = 1;
    } else {
        evt.type = -1; // not supported even type
    }
    sec_flag = 1;
    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    TIMERG0.hw_timer[timer_idx].config.alarm_en = TIMER_ALARM_EN;

    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(timer_queue, &evt, NULL);
}

/*
 * Initialize selected timer of the timer group 0
 *
 * timer_idx - the timer number to initialize
 * auto_reload - should the timer auto reload on alarm?
 * timer_interval_sec - the interval of alarm to set
 */
static void example_tg0_timer_init(int timer_idx,
    bool auto_reload, double timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = auto_reload;
    timer_init(TIMER_GROUP_0, timer_idx, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, timer_idx, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, timer_idx, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(TIMER_GROUP_0, timer_idx);
    timer_isr_register(TIMER_GROUP_0, timer_idx, timer_group0_isr,
        (void *) timer_idx, ESP_INTR_FLAG_IRAM, NULL);

    timer_start(TIMER_GROUP_0, timer_idx);
}


//----------------------
static void _checkTime()
{
	time(&time_now);
	if (time_now > time_last) {
		color_t last_fg, last_bg;
		time_last = time_now;
		tm_info = localtime(&time_now);
		sprintf(tmp_buff, "%04d-%02d-%02d %02d:%02d:%02d",
				(tm_info->tm_year + 1900), tm_info->tm_mon + 1, tm_info->tm_mday,
				tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
		TFT_saveClipWin();
		TFT_resetclipwin();

		Font curr_font = cfont;
		last_bg = _bg;
		last_fg = _fg;
		_fg = TFT_YELLOW;
		_bg = (color_t){ 64, 64, 64 };
		TFT_setFont(DEFAULT_FONT, NULL);

		TFT_fillRect(1, _height-TFT_getfontheight()-8, _width-3, TFT_getfontheight()+6, _bg);
		TFT_print(tmp_buff, CENTER, _height-TFT_getfontheight()-5);

		cfont = curr_font;
		_fg = last_fg;
		_bg = last_bg;

		TFT_restoreClipWin();
		print_display();
	}
}

//---------------------
static int Wait(int ms)
{
	uint8_t tm = 1;
	if (ms < 0) {
		tm = 0;
		ms *= -1;
	}
	if (ms <= 50) {
		vTaskDelay(ms / portTICK_RATE_MS);
		//if (_checkTouch()) return 0;
	}
	else {
		for (int n=0; n<ms; n += 50) {
			vTaskDelay(50 / portTICK_RATE_MS);
			if (tm) {
				_checkTime();
			}
			//if (_checkTouch()) return 0;
		}
	}
	return 1;
}

static void print_display()
{	char buf[3]; int w; int SIZE_7SEG;
	TFT_setFont(FONT_7SEG, NULL);

	TFT_resetclipwin();
	font_transparent = 0;

	_fg = TFT_RED;
	_bg = TFT_BLACK;

//	tm_info = localtime(&time_now);

	if (disp_rot == PORTRAIT)
	{
		SIZE_7SEG = 14;
	} else SIZE_7SEG = 20;

	set_7seg_font_atrib(SIZE_7SEG, 4, 2, TFT_RED);

	TFT_saveClipWin();
	TFT_resetclipwin();

//	int h = TFT_getfontheight();

	if (tm_info->tm_sec > last_sec)
	{
		if (last_sec % 2)
		{
			sprintf(buf, ":");
		} else
		{
			sprintf(buf, "-");
		}

		w = 235 - 100;
		TFT_clearStringRect(w, 30, buf);
		TFT_print(buf, w, 30);

//	    refresh(38);

	}

	if (tm_info->tm_min > last_min)
	{
		sprintf(buf, "%02d", tm_info->tm_min);
		w = TFT_getStringWidth(buf);
		printf("w = %d\r\n", w);
		w = 235 - w;
		TFT_clearStringRect(w, 30, buf);
		TFT_print(buf, w, 30);
	}

	if (tm_info->tm_hour > last_hour)
	{
		sprintf(buf, "%02d", tm_info->tm_hour);
		w = TFT_getStringWidth(buf);
		w = 235 - ((2 * w) + 36);
		TFT_clearStringRect(w, 30, buf);
		TFT_print(buf, w, 30);
	}

	last_hour = tm_info->tm_hour;
	last_min = tm_info->tm_min;
	last_sec = tm_info->tm_sec;

	TFT_restoreClipWin();

//	TFT_fillRect(TIME_X_COORD, _height-TFT_getfontheight()-9, 80, TFT_getfontheight()+8, _bg);
//	TFT_drawRect(80, _height-TFT_getfontheight()-9, _width-80-1, TFT_getfontheight()+8, TFT_CYAN);


/*	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
   	printf("\n_dispTime():%s;\n", tmp_buff);


	TFT_print(tmp_buff, RIGHT, _height-TFT_getfontheight()-5);
   	printf("TFT_print(tmp_buff=%s, CENTER=%d, _height-TFT_getfontheight()-5=%d);\n", tmp_buff,
   			CENTER, _height-TFT_getfontheight()-5);
    cfont = curr_font;*/

}

//---------------------
static void _dispTime()
{

	TFT_setFont(DEFAULT_FONT, NULL);

	TFT_resetclipwin();
	font_transparent = 1;

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };
	TFT_fillRect(TIME_X_COORD, _height-TFT_getfontheight()-9, 80, TFT_getfontheight()+8, _bg);
//	TFT_drawRect(80, _height-TFT_getfontheight()-9, _width-80-1, TFT_getfontheight()+8, TFT_CYAN);


	Font curr_font = cfont;
    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

    time(&time_now);
	time_last = time_now;
	tm_info = localtime(&time_now);
	sprintf(tmp_buff, "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
   	printf("\n_dispTime():%s;\n", tmp_buff);


	TFT_print(tmp_buff, RIGHT, _height-TFT_getfontheight()-5);
   	printf("TFT_print(tmp_buff=%s, CENTER=%d, _height-TFT_getfontheight()-5=%d);\n", tmp_buff,
   			CENTER, _height-TFT_getfontheight()-5);
    cfont = curr_font;

}

//---------------------------------
static void disp_header(char *info)
{
	TFT_fillScreen(TFT_BLACK);
	TFT_resetclipwin();

	_fg = TFT_YELLOW;
	_bg = (color_t){ 64, 64, 64 };

    if (_width < 240) TFT_setFont(DEF_SMALL_FONT, NULL);
	else TFT_setFont(DEFAULT_FONT, NULL);

	TFT_fillRect(0, 0, _width-1, TFT_getfontheight()+8, _bg);
	TFT_drawRect(0, 0, _width-1, TFT_getfontheight()+8, TFT_CYAN);

	TFT_print(info, CENTER, 4);

	_bg = TFT_BLACK;
	TFT_setclipwin(0,TFT_getfontheight()+9, _width-1, _height-TFT_getfontheight()-10);
}


//------------------------
static void test_times() {

	if (doprint) {
	    uint32_t tstart, t1, t2;
		disp_header("TIMINGS");
		// ** Show Fill screen and send_line timings
		tstart = clock();
		TFT_fillWindow(TFT_BLACK);
		t1 = clock() - tstart;
		printf("     Clear screen time: %u ms\r\n", t1);
		TFT_setFont(SMALL_FONT, NULL);
		sprintf(tmp_buff, "Clear screen: %u ms", t1);
		TFT_print(tmp_buff, 0, 140);

		color_t *color_line = heap_caps_malloc((_width*3), MALLOC_CAP_DMA);
		color_t *gsline = NULL;
		if (gray_scale) gsline = malloc(_width*3);
		if (color_line) {
			float hue_inc = (float)((10.0 / (float)(_height-1) * 360.0));
			for (int x=0; x<_width; x++) {
				color_line[x] = HSBtoRGB(hue_inc, 1.0, (float)x / (float)_width);
				if (gsline) gsline[x] = color_line[x];
			}
			disp_select();
			tstart = clock();
			for (int n=0; n<1000; n++) {
				if (gsline) memcpy(color_line, gsline, _width*3);
				send_data(0, 40+(n&63), dispWin.x2-dispWin.x1, 40+(n&63), (uint32_t)(dispWin.x2-dispWin.x1+1), color_line);
				wait_trans_finish(1);
			}
			t2 = clock() - tstart;
			disp_deselect();

			printf("Send color buffer time: %u us (%d pixels)\r\n", t2, dispWin.x2-dispWin.x1+1);
			free(color_line);

			sprintf(tmp_buff, "   Send line: %u us", t2);
			TFT_print(tmp_buff, 0, 144+TFT_getfontheight());
		}
		Wait(GDEMO_INFO_TIME);
    }
}

//===============
void tft_demo() {

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	TFT_resetclipwin();

	image_debug = 0;

    char dtype[16];
    
    switch (tft_disp_type) {
        case DISP_TYPE_ILI9341:
            sprintf(dtype, "ILI9341");
            break;
        case DISP_TYPE_ILI9488:
            sprintf(dtype, "ILI9488");
            break;
        case DISP_TYPE_ST7789V:
            sprintf(dtype, "ST7789V");
            break;
        case DISP_TYPE_ST7735:
            sprintf(dtype, "ST7735");
            break;
        case DISP_TYPE_ST7735R:
            sprintf(dtype, "ST7735R");
            break;
        case DISP_TYPE_ST7735B:
            sprintf(dtype, "ST7735B");
            break;
        default:
            sprintf(dtype, "Unknown");
    }
    
    uint8_t disp_rot = PORTRAIT;
	_demo_pass = 0;
	gray_scale = 0;
	doprint = 1;

	TFT_setRotation(disp_rot);
	disp_header("ESP32 TFT DEMO");
	TFT_setFont(COMIC24_FONT, NULL);
	int tempy = TFT_getfontheight() + 4;
	_fg = TFT_ORANGE;
	TFT_print("ESP32", CENTER, (dispWin.y2-dispWin.y1)/2 - tempy);
	TFT_setFont(UBUNTU16_FONT, NULL);
	_fg = TFT_CYAN;
	TFT_print("TFT Demo", CENTER, LASTY+tempy);
	tempy = TFT_getfontheight() + 4;
	TFT_setFont(DEFAULT_FONT, NULL);
	_fg = TFT_GREEN;
	sprintf(tmp_buff, "Read speed: %5.2f MHz", (float)max_rdclock/1000000.0);
	TFT_print(tmp_buff, CENTER, LASTY+tempy);

	Wait(400);

	disp_header("Welcome to ESP32");

	while (1) {
		if (run_gs_demo) {
			if (_demo_pass == 8) doprint = 0;
			// Change gray scale mode on every 2nd pass
			gray_scale = _demo_pass & 1;
			// change display rotation
			if ((_demo_pass % 2) == 0) {
				_bg = TFT_BLACK;
				TFT_setRotation(disp_rot);
				disp_rot++;
				disp_rot &= 3;
			}
		}
		else {
			if (_demo_pass == 4) doprint = 0;
			// change display rotation
			_bg = TFT_BLACK;
			TFT_setRotation(disp_rot);
			disp_rot++;
			disp_rot &= 3;
		}

		if (doprint) {
			if (disp_rot == 1) sprintf(tmp_buff, "PORTRAIT");
			if (disp_rot == 2) sprintf(tmp_buff, "LANDSCAPE");
			if (disp_rot == 3) sprintf(tmp_buff, "PORTRAIT FLIP");
			if (disp_rot == 0) sprintf(tmp_buff, "LANDSCAPE FLIP");
			printf("\r\n==========================================\r\nDisplay: %s: %s %d,%d %s\r\n\r\n",
					dtype, tmp_buff, _width, _height, ((gray_scale) ? "Gray" : "Color"));
		}

		test_times();
	}
}


//=============
void app_main()
{
    esp_err_t ret;

    tft_disp_type = DEFAULT_DISP_TYPE;
	_width = DEFAULT_TFT_DISPLAY_WIDTH;  // smaller dimension
	_height = DEFAULT_TFT_DISPLAY_HEIGHT; // larger dimension
	max_rdclock = 8000000;
    TFT_PinsInit();

    // ====  CONFIGURE SPI DEVICES(s)  ====================================================================================

    spi_lobo_device_handle_t spi;
	
    spi_lobo_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,				// set SPI MISO pin
        .mosi_io_num=PIN_NUM_MOSI,				// set SPI MOSI pin
        .sclk_io_num=PIN_NUM_CLK,				// set SPI CLK pin
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
		.max_transfer_sz = 6*1024,
    };
    spi_lobo_device_interface_config_t devcfg={
        .clock_speed_hz=8000000,                // Initial clock out at 8 MHz
        .mode=0,                                // SPI mode 0
        .spics_io_num=-1,                       // we will use external CS pin
		.spics_ext_io_num=PIN_NUM_CS,           // external CS pin
		.flags=SPI_DEVICE_HALFDUPLEX,           // ALWAYS SET  to HALF DUPLEX MODE!! for display spi
    };

#if USE_TOUCH == 1
    spi_lobo_device_handle_t tsspi = NULL;

    spi_lobo_device_interface_config_t tsdevcfg={
        .clock_speed_hz=2500000,                //Clock out at 2.5 MHz
        .mode=0,                                //SPI mode 0
        .spics_io_num=PIN_NUM_TCS,              //Touch CS pin
		.spics_ext_io_num=-1,                   //Not using the external CS
		.command_bits=8,                        //1 byte command
    };
#endif
    // ====================================================================================================================

    /* */

//    vTaskDelay(500 / portTICK_RATE_MS);
	printf("\r\n==============================\r\n");
    printf("TFT display DEMO, LoBo 10/2017\r\n");
	printf("==============================\r\n");
    printf("Pins used: miso=%d, mosi=%d, sck=%d, cs=%d\r\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
	printf("==============================\r\n\r\n");

	// ==================================================================
	// ==== Initialize the SPI bus and attach the LCD to the SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &devcfg, &spi);
    assert(ret==ESP_OK);
	printf("SPI: display device added to spi bus (%d)\r\n", SPI_BUS);
	disp_spi = spi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(spi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(spi);
    assert(ret==ESP_OK);

	printf("SPI: attached display device, speed=%u\r\n", spi_lobo_get_speed(spi));
	printf("SPI: bus uses native pins: %s\r\n", spi_lobo_uses_native_pins(spi) ? "true" : "false");

/* ------------ BLUETOOTH INITIALIZE --------------------------------*/

	ESP_LOGE(tag, "------------ BLUETOOTH INITIALIZE --------------------------------\r\n");



#if USE_TOUCH
	// =====================================================
    // ==== Attach the touch screen to the same SPI bus ====

	ret=spi_lobo_bus_add_device(SPI_BUS, &buscfg, &tsdevcfg, &tsspi);
    assert(ret==ESP_OK);
	printf("SPI: touch screen device added to spi bus (%d)\r\n", SPI_BUS);
	ts_spi = tsspi;

	// ==== Test select/deselect ====
	ret = spi_lobo_device_select(tsspi, 1);
    assert(ret==ESP_OK);
	ret = spi_lobo_device_deselect(tsspi);
    assert(ret==ESP_OK);namespace {

	printf("SPI: attached TS device, speed=%u\r\n", spi_lobo_get_speed(tsspi));
#endif

	// ================================
	// ==== Initialize the Display ====

	printf("SPI: display init...\r\n");
	TFT_display_init();
    printf("OK\r\n");
	
	// ---- Detect maximum read speed ----
	max_rdclock = find_rd_speed();
	printf("SPI: Max rd speed = %u\r\n", max_rdclock);

    // ==== Set SPI clock used for display operations ====
	spi_lobo_set_speed(spi, DEFAULT_SPI_CLOCK);
	printf("SPI: Changed speed to %u\r\n", spi_lobo_get_speed(spi));

    printf("\r\n---------------------\r\n");
	printf("Graphics demo started\r\n");
	printf("---------------------\r\n");

    printf("\r\n---------------------\r\n");
	printf("Timer queue, and interrupt init\r\n");
	printf("---------------------\r\n");
    timer_queue = xQueueCreate(10, sizeof(timer_event_t));
    example_tg0_timer_init(TIMER_0, TEST_WITH_RELOAD, TIMER_INTERVAL0_SEC);

//    xTaskCreate(timer_example_evt_task, "timer_evt_task", 2048, NULL, 5, NULL);

	font_rotate = 0;
	text_wrap = 0;
	font_transparent = 0;
	font_forceFixed = 0;
	gray_scale = 0;
    TFT_setGammaCurve(DEFAULT_GAMMA_CURVE);
	TFT_setRotation(disp_rot);
	TFT_setFont(DEFAULT_FONT, NULL);
	TFT_resetclipwin();

#ifdef CONFIG_EXAMPLE_USE_WIFI

	// ===== Set time zone ======
	setenv("TZ", "CET-1CEST", 0);
	tzset();
	// ==========================

	disp_header("GET NTP TIME");

    time(&time_now);
	tm_info = localtime(&time_now);

	// Is time set? If not, tm_year will be (1970 - 1900).
    if (tm_info->tm_year < (2016 - 1900)) {
        ESP_LOGI(tag, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        _fg = TFT_CYAN;
    	TFT_print("Time is not set yet", CENTER, CENTER);
    	TFT_print("Connecting to WiFi", CENTER, LASTY+TFT_getfontheight()+2);
    	TFT_print("Getting time over NTP", CENTER, LASTY+TFT_getfontheight()+2);
    	_fg = TFT_YELLOW;
    	TFT_print("Wait", CENTER, LASTY+TFT_getfontheight()+2);
        if (obtain_time()) {
        	_fg = TFT_GREEN;
        	TFT_print("System time is set.", CENTER, LASTY);
        }
        else {
        	_fg = TFT_RED;
        	TFT_print("ERROR.", CENTER, LASTY);
        }
        time(&time_now);
//    	update_header(NULL, "");
    	Wait(-2000);
    }
#endif


	disp_header("File system INIT");
    _fg = TFT_CYAN;
	TFT_print("Initializing SPIFFS...", CENTER, CENTER);
    // ==== Initialize the file system ====
    printf("\r\n\n");
	vfs_spiffs_register();
    if (!spiffs_is_mounted) {
    	_fg = TFT_RED;
    	TFT_print("SPIFFS not mounted !", CENTER, LASTY+TFT_getfontheight()+2);
    }
    else {
    	_fg = TFT_GREEN;	_dispTime();

    	TFT_print("SPIFFS Mounted.", CENTER, LASTY+TFT_getfontheight()+2);
    }
    disp_header("Display Header");

    init_slide(&slide01, 10, 100, 64, 190, 10);
    set_slide_colors(&slide01, TFT_BLUE, TFT_BLACK, TFT_LIGHTGREY);
    refresh(&slide01, 38);

    init_slide(&slide02, 80, 100, 64, 190, 10);
    set_slide_colors(&slide02, TFT_BLUE, TFT_BLACK, TFT_LIGHTGREY);
    refresh(&slide02, 50);

    init_slide(&slide03, 150, 100, 64, 190, 10);
    set_slide_colors(&slide03, TFT_BLUE, TFT_BLACK, TFT_LIGHTGREY);
    refresh(&slide03, 75);


    Wait(1000);

    while (1)
    {

    	Wait(1000);
/*        if (sec_flag)
        {
        	printf("sec_flag = 1\n\r");
//        	_dispTime();
//        	print_display();
        	sec_flag = 0;
        }*/
    }



}




