#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include "keys.h"

//The path to the keyboard and mouse (should look something like /dev/input/eventX)
char KEYBOARD_PATH[] = "<YOUR-PATH>";
char MOUSE_PATH[] = "<YOUR-PATH>";


bool q_pressed = false;

int absXLEFT = 0;
int absXRIGHT = 0;
int absYUP = 0;
int absYDOWN = 0;

int MOUSE_SENSITIVITY = 299;
int MOUSE_SENSITIVITY_NEGATIVE = -299;

void exitFunc(int keyboard_fd, int mouse_fd, int gamepad_fd)
{
  close(keyboard_fd);
  close(mouse_fd);
  if (ioctl(gamepad_fd, UI_DEV_DESTROY) < 0)
  {
    printf("Error destroying gamepad! \n");
  }
  close(gamepad_fd);
}

void send_sync_event(int gamepad_fd, struct input_event gamepad_event)
{
    memset(&gamepad_event, 0, sizeof(struct input_event));
    gamepad_event.type = EV_SYN;
    gamepad_event.code = 0;
    gamepad_event.value = 0;

    if(write(gamepad_fd, &gamepad_event, sizeof(struct input_event)) < 0)
    {
      printf("error writing sync event\n");
    }
}

void send_keyboard_event(int gamepad_fd, struct input_event gamepad_event, int TYPE, int CODE, int VALUE) // TYPE Is The event to write to the gamepad and CODE is an integer value for the button on the gamepad
{
  memset(&gamepad_event, 0, sizeof(struct input_event));
  gamepad_event.type = TYPE;
  gamepad_event.code = CODE;
  gamepad_event.value = VALUE;

  if(write(gamepad_fd, &gamepad_event, sizeof(struct input_event)) < 0)
  {
    printf("Error writing keyboard event to gamepad!\n");
  }
}

void send_mouse_event(int gamepad_fd, struct input_event gamepad_event, int TYPE, int CODE, int VALUE)
{
  memset(&gamepad_event, 0, sizeof(struct input_event));
  gamepad_event.type = TYPE;
  gamepad_event.code = CODE;
  gamepad_event.value = VALUE;

  if(write(gamepad_fd, &gamepad_event, sizeof(struct input_event)) < 0)
  {
    printf("Error writing mouse event to gamepad!\n");
  }
}

int main(int argc, char *argv[])
{
  sleep(1);
  int rcode = 0;

  char keyboard_name[256] = "Unknown";
  int keyboard_fd = open(KEYBOARD_PATH, O_RDONLY | O_NONBLOCK);
  if (keyboard_fd == -1)
  {
    printf("Failed to open keyboard.\n");
    exit(1);
  }
  rcode = ioctl(keyboard_fd, EVIOCGNAME(sizeof(keyboard_name)), keyboard_name);
  printf("Reading From : %s \n", keyboard_name);

  //   printf("Getting exclusive access: ");
  //   rcode = ioctl(keyboard_fd, EVIOCGRAB, 1);
  //   printf("%s\n", (rcode == 0) ? "SUCCESS" : "FAILURE");>>
  struct input_event keyboard_event;

  char mouse_name[256] = "Unknown";
  int mouse_fd = open(MOUSE_PATH, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (mouse_fd == -1)
  {
    printf("Failed to open mouse.\n");
    exit(1);
  }

  rcode = ioctl(mouse_fd, EVIOCGNAME(sizeof(mouse_name)), mouse_name);
  printf("Reading From : %s \n", mouse_name);

  struct input_event mouse_event;

  // printf("Getting exclusive access: ");
  // rcode = ioctl(mouse_fd, EVIOCGRAB, 1);
  // printf("%s\n", (rcode == 0) ? "SUCCESS" : "FAILURE");

  // Now, create gamepad

  int gamepad_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (gamepad_fd < 0)
  {
    printf("Opening of input failed! \n");
    return 1;
  }

  ioctl(gamepad_fd, UI_SET_EVBIT, EV_KEY); // setting Gamepad keys
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_A);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_B);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_X);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_Y);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TL);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TR);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TL2);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_TR2);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_START);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_SELECT);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_THUMBL);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_THUMBR);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_DPAD_UP);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_DPAD_DOWN);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_DPAD_LEFT);
  ioctl(gamepad_fd, UI_SET_KEYBIT, BTN_DPAD_RIGHT);

  ioctl(gamepad_fd, UI_SET_EVBIT, EV_ABS); // setting Gamepad thumbsticks
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_X);
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_Y);
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_RX);
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_RY);
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_TILT_X);
  ioctl(gamepad_fd, UI_SET_ABSBIT, ABS_TILT_Y);


  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Gamepad"); // Name of Gamepad
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x3;
  uidev.id.product = 0x3;
  uidev.id.version = 2;
  uidev.absmax[ABS_X] = 32767; // Parameters of thumbsticks
  uidev.absmin[ABS_X] = -32768;
  uidev.absfuzz[ABS_X] = 0;
  uidev.absflat[ABS_X] = 15;
  uidev.absmax[ABS_Y] = 32767;
  uidev.absmin[ABS_Y] = -32768;
  uidev.absfuzz[ABS_Y] = 0;
  uidev.absflat[ABS_Y] = 15;
  uidev.absmax[ABS_RX] = 512;
  uidev.absmin[ABS_RX] = -512;
  uidev.absfuzz[ABS_RX] = 0;
  uidev.absflat[ABS_RX] = 16;
  uidev.absmax[ABS_RY] = 512;
  uidev.absmin[ABS_RY] = -512;
  uidev.absfuzz[ABS_RY] = 0;
  uidev.absflat[ABS_RY] = 16;

  if (write(gamepad_fd, &uidev, sizeof(uidev)) < 0)
  {
    printf("Failed to write! \n");
    return 1;
  }
  if (ioctl(gamepad_fd, UI_DEV_CREATE) < 0)
  {
    printf("Failed to create gamepad! \n");
    return 1;
  }

  sleep(0.6);

  struct input_event gamepad_ev;

  while (1)
  {
    if (read(keyboard_fd, &keyboard_event, sizeof(keyboard_event)) != -1)
    {
      // printf("keyboard event: type %d code %d value %d  \n", keyboard_event.type, keyboard_event.code, keyboard_event.value);
      if (keyboard_event.code == KEY_Q && keyboard_event.value == 1)
      {
        q_pressed = !q_pressed;
      }
      if (keyboard_event.code == KEY_ENTER && keyboard_event.value == 1 && q_pressed == true)
      {
        exitFunc(keyboard_fd, mouse_fd, gamepad_fd);
        break;
      }
      // enter to A
      if (keyboard_event.code == KEY_ENTER && keyboard_event.value != 2) // only care about button press and not hold
      {

        printf(__STRING(KEY_ENTER));
        printf(" pressed"
               "\n");

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_A, keyboard_event.value);
        
        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // G to Y
      if (keyboard_event.code == KEY_G && keyboard_event.value != 2) // only care about button press
      {

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_Y, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // E to X(interact)
      if (keyboard_event.code == KEY_E && keyboard_event.value != 2) // only care about button press
      {

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_X, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // spacebar to A(jump)
      if (keyboard_event.code == KEY_SPACE && keyboard_event.value != 2) // only care about button press
      {

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_A, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // J to X axis move right in increments
      if (keyboard_event.code == KEY_J)
      {
        absXRIGHT++;
        if (absXRIGHT >= 32767)
        {
          absXRIGHT = 32767;
        }

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_X, absXRIGHT);
        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // L to X axis move left in increments
      if (keyboard_event.code == KEY_L)
      {
        absXLEFT--;
        if (absXLEFT <= -32768)
        {
          absXLEFT = -32768;
        }
        
        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_X, absXLEFT);
        
        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // I to Y axis move up inincrements
      if (keyboard_event.code == KEY_I)
      {
        absYUP--;

        if (absYUP <= -32768)
        {
          absYUP = -32768;
        }
        
        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_Y, absYUP);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // J to Y axis move down in increments
      if (keyboard_event.code == KEY_J)
      {
        absYDOWN++;
        if (absYDOWN >= 32767)
        {
          absYDOWN = 32767;
        }

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_Y, absYDOWN);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // W to Y axis move up(forward) full speed
      if (keyboard_event.code == KEY_W)
      {
        int toWrite = 0;
        if (keyboard_event.value == 2 || keyboard_event.value == 1) // on pressed or on hold key
          toWrite = -32768;

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_Y, toWrite);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // S to Y move down(backwards) full speed
      if (keyboard_event.code == KEY_S)
      {
        int toWrite = 0;
        if (keyboard_event.value == 2 || keyboard_event.value == 1) // on pressed or on hold key
          toWrite = 32767;

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_Y, toWrite);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      if (keyboard_event.code == KEY_A)
      {
        int toWrite = 0;
        if (keyboard_event.value == 2 || keyboard_event.value == 1) // on pressed or on hold key
          toWrite = -32768;

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_X, toWrite);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      if (keyboard_event.code == KEY_D)
      {

        int toWrite = 0;
        if (keyboard_event.value == 2)
          toWrite = 32767;

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_X, toWrite);

        // send sync 
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // C to B(cancel)
      if (keyboard_event.code == KEY_C && keyboard_event.value != 2) // only care about button press
      {

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_B, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // V to left stick
      if (keyboard_event.code == KEY_LEFTSHIFT)
      {
        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_THUMBL, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // B to right stick
      if (keyboard_event.code == KEY_RIGHTSHIFT)
      {
        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_THUMBR, keyboard_event.value);

        // send sync event
        send_sync_event(gamepad_fd, gamepad_ev);
      }

      // reset view joystick on left control
      if (keyboard_event.code == KEY_LEFTCTRL)
      {
        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RX, 0);

        //send keyboard event to gamepad
        send_keyboard_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RY, 0);

        // send one sync event for both axes
        send_sync_event(gamepad_fd, gamepad_ev);
      }
    }

    if (int sz = read(mouse_fd, &mouse_event, sizeof(struct input_event)))
    {
      if (sz != -1)
      {
        // printf("Mouse event: %d %d %d \n", mouse_event.code, mouse_event.type, mouse_event.value);
        switch (mouse_event.type)
        {
        case EV_REL:
          if (mouse_event.code == REL_X)
          {
            int toWrite = 0;
            if (mouse_event.value > 0)
            {
              toWrite = MOUSE_SENSITIVITY;
            }
            if (mouse_event.value < 0)
            {
              toWrite = MOUSE_SENSITIVITY_NEGATIVE;
            }
            if (mouse_event.value == 0)
            {
              toWrite = 0;
            }

            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RX, toWrite);

            // send sync event
            send_sync_event(gamepad_fd, gamepad_ev);
          }
          if (mouse_event.code == REL_Y)
          {
            int toWrite = 0;
            if (mouse_event.value > 0)
            {
              toWrite = MOUSE_SENSITIVITY;
            }
            if (mouse_event.value < 0)
            {
              toWrite = MOUSE_SENSITIVITY_NEGATIVE;
            }
            if (mouse_event.value == 0)
            {
              toWrite = 0;
            }

            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RY, toWrite);

            // send sync event
            send_sync_event(gamepad_fd, gamepad_ev);
          }
          break;
        case EV_KEY:
          if (mouse_event.code == BTN_LEFT)
          {
            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_TL2, mouse_event.value);
            // send sync event
            send_sync_event(gamepad_fd, gamepad_ev);
          }
          // reset controller state
          if (mouse_event.code == BTN_MIDDLE)
          {
            printf("Middle button of mouse clicked!\n");
            
            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RX, 0);

            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_ABS, ABS_RY, 0);

            // send one sync event for both axes
            send_sync_event(gamepad_fd, gamepad_ev);
          }
          if (mouse_event.code == BTN_RIGHT)
          {
            //send mouse event to gamepad
            send_mouse_event(gamepad_fd, gamepad_ev, EV_KEY, BTN_TR2, mouse_event.value);
             
            // send sync event
            send_sync_event(gamepad_fd, gamepad_ev);
          }
          break;
        }
      }
    }
  }

  printf("Exiting.\n");
  rcode = ioctl(keyboard_fd, EVIOCGRAB, 1);
  close(keyboard_fd);
  rcode = ioctl(mouse_fd, EVIOCGRAB, 1);
  close(mouse_fd);
  return 0;
}
