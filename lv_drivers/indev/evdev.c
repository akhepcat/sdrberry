/**
 * @file evdev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "evdev.h"
#if USE_EVDEV != 0 || USE_BSD_EVDEV

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#if USE_BSD_EVDEV
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max);

/**********************
 *  STATIC VARIABLES
 **********************/
int evdev_fd;
int evdev_root_x;
int evdev_root_y;
int evdev_button;

int evdev_key_val;
int swap_xy = 0;
int debug_xy = 0;
char touch_driver[80];
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void evdev_touch_swap(int swap, int debug)
{
	swap_xy = swap;
	debug_xy = debug;
}

void evdev_touch_driver(const char *driver)
{
	strcpy(touch_driver, driver);
}
/**
 * Initialize the evdev interface
 */
void evdev_init()
{
	char str[2048];
	char name[256] = "Unknown";
	int input_device = 0;
	char *ptr;

	// search for touch screen
	// first check which device need to be opened.
	do
	{
		sprintf(str, "/dev/input/event%d", input_device);
		evdev_fd = open(str, O_RDONLY | O_NONBLOCK);
		if (evdev_fd > 0)
		{
			ioctl(evdev_fd, EVIOCGNAME(sizeof(name)), name);
			printf("Input device name: \"%s\"\n", name);
			ptr = strstr(name, "raspberrypi-ts");
			if (ptr == NULL) // Bullseye changed driver name
			    ptr = strstr(name, "ft5x06");
			if (ptr == NULL) // Bullseye changed driver name
				ptr = strstr(name, "WS Capacitive TouchScreen");
			if (ptr == NULL) // Bullseye changed driver name
				ptr = strstr(name, "Goodix Capacitive TouchScreen");
			if (ptr == NULL) // Bullseye changed driver name
				ptr = strstr(name, "QDtech");
			if (ptr == NULL) // Bullseye changed driver name
				ptr = strstr(name, "ILITEK");
			if (ptr == NULL && strlen(touch_driver))
				ptr = strstr(name, touch_driver);
			if (ptr == NULL)
			{
				close(evdev_fd);
				input_device++;
				evdev_fd = -1;
			}
		}
		else
			input_device++;
	} while (input_device < 30 && evdev_fd == -1) ;
	printf("Touch device name: \"%s\"\n", name);

	if(evdev_fd == -1) {
        perror("unable open evdev interface:");
        return;
    }

#if USE_BSD_EVDEV
    fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
    fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif

    evdev_root_x = 0;
    evdev_root_y = 0;
    evdev_key_val = 0;
    evdev_button = LV_INDEV_STATE_REL;
}
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char* dev_name)
{ 
     if(evdev_fd != -1) {
        close(evdev_fd);
     }
#if USE_BSD_EVDEV
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY);
#else
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

     if(evdev_fd == -1) {
        perror("unable open evdev interface:");
        return false;
     }

#if USE_BSD_EVDEV
     fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
     fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif

     evdev_root_x = 0;
     evdev_root_y = 0;
     evdev_key_val = 0;
     evdev_button = LV_INDEV_STATE_REL;

     return true;
}
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 * @return false: because the points are not buffered, so no more data to be read
 */
void evdev_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    struct input_event in;

    while(read(evdev_fd, &in, sizeof(struct input_event)) > 0) {
        if(in.type == EV_REL) {
            if(in.code == REL_X)
				#if EVDEV_SWAP_AXES
					evdev_root_y += in.value;
				#else
					evdev_root_x += in.value;
				#endif
            else if(in.code == REL_Y)
				#if EVDEV_SWAP_AXES
					evdev_root_x += in.value;
				#else
					evdev_root_y += in.value;
				#endif
        } else if(in.type == EV_ABS) {
            if(in.code == ABS_X)
				#if EVDEV_SWAP_AXES
					evdev_root_y = in.value;
				#else
					evdev_root_x = in.value;
				#endif
            else if(in.code == ABS_Y)
				#if EVDEV_SWAP_AXES
					evdev_root_x = in.value;
				#else
					evdev_root_y = in.value;
				#endif
            else if(in.code == ABS_MT_POSITION_X)
                                #if EVDEV_SWAP_AXES
                                        evdev_root_y = in.value;
                                #else
                                        evdev_root_x = in.value;
                                #endif
            else if(in.code == ABS_MT_POSITION_Y)
                                #if EVDEV_SWAP_AXES
                                        evdev_root_x = in.value;
                                #else
                                        evdev_root_y = in.value;
                                #endif
            else if(in.code == ABS_MT_TRACKING_ID)
                                if(in.value == -1)
                                    evdev_button = LV_INDEV_STATE_REL;
                                else if(in.value == 0)
                                    evdev_button = LV_INDEV_STATE_PR;
        } else if(in.type == EV_KEY) {
            if(in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
                if(in.value == 0)
                    evdev_button = LV_INDEV_STATE_REL;
                else if(in.value == 1)
                    evdev_button = LV_INDEV_STATE_PR;
            } else if(drv->type == LV_INDEV_TYPE_KEYPAD) {
		data->state = (in.value) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
		switch(in.code) {
			case KEY_BACKSPACE:
				data->key = LV_KEY_BACKSPACE;
				break;
			case KEY_ENTER:
				data->key = LV_KEY_ENTER;
				break;
			case KEY_UP:
				data->key = LV_KEY_UP;
				break;
			case KEY_LEFT:
				data->key = LV_KEY_PREV;
				break;
			case KEY_RIGHT:
				data->key = LV_KEY_NEXT;
				break;
			case KEY_DOWN:
				data->key = LV_KEY_DOWN;
				break;
			default:
				data->key = 0;
				break;
		}
		evdev_key_val = data->key;
		evdev_button = data->state;
		return ;
	    }
        }
    }

    if(drv->type == LV_INDEV_TYPE_KEYPAD) {
        /* No data retrieved */
        data->key = evdev_key_val;
	data->state = evdev_button;
	return ;
    }
    if(drv->type != LV_INDEV_TYPE_POINTER)
        return ;
    /*Store the collected data*/

#if EVDEV_CALIBRATE
    data->point.x = map(evdev_root_x, EVDEV_HOR_MIN, EVDEV_HOR_MAX, 0, drv->disp->driver->hor_res);
    data->point.y = map(evdev_root_y, EVDEV_VER_MIN, EVDEV_VER_MAX, 0, drv->disp->driver->ver_res);
#else
    data->point.x = evdev_root_x;
    data->point.y = evdev_root_y;
#endif

    data->state = evdev_button;

	// xy swap 
	if (swap_xy == 1)
	{
		int x = data->point.x;
		data->point.x = data->point.y;
		data->point.y = x;
	}

	if (swap_xy == 4)
	{
		int x = data->point.x;
		data->point.x = data->point.y;
		data->point.y = x;

		data->point.x = drv->disp->driver->hor_res - data->point.x;
		data->point.y = drv->disp->driver->ver_res - data->point.y;
	}

	// x inverted
	if (swap_xy == 2)
	{
		data->point.x = drv->disp->driver->hor_res - data->point.x;
	}

	// Roatate touch
	if (swap_xy == 3)
	{
		data->point.x = drv->disp->driver->hor_res - data->point.x;
		data->point.y = drv->disp->driver->ver_res - data->point.y;
	}

	if ((data->state == LV_INDEV_STATE_PR) && (data->point.x < 0 || data->point.y < 0 || data->point.x >= drv->disp->driver->hor_res || data->point.y >= drv->disp->driver->ver_res))
	  printf("touch x: %d y %d hor_res %d ver_res %d\n", data->point.x, data->point.y, drv->disp->driver->hor_res, drv->disp->driver->ver_res);

	if(data->point.x < 0)
      data->point.x = 0;
    if(data->point.y < 0)
      data->point.y = 0;
    if(data->point.x >= drv->disp->driver->hor_res)
      data->point.x = drv->disp->driver->hor_res - 1;
    if(data->point.y >= drv->disp->driver->ver_res)
      data->point.y = drv->disp->driver->ver_res - 1;

	if (data->state == LV_INDEV_STATE_PR && debug_xy)
		printf("touch x: %d y %d hor_res %d ver_res %d\n", data->point.x, data->point.y, drv->disp->driver->hor_res, drv->disp->driver->ver_res);

	return ;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif
