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
#include <signal.h>
#include <poll.h>
#include <stdbool.h>

// Keep file descriptors global so the asynchronous signal handler can clean them up
volatile sig_atomic_with_t keep_running = 1;
int global_keyboard_fd = -1;
int global_mouse_fd = -1;
int global_gamepad_fd = -1;

// The path to the keyboard and mouse (Update these to your event nodes)
char KEYBOARD_PATH[] = "/dev/input/event0"; 
char MOUSE_PATH[] = "/dev/input/event1";

bool q_pressed = false;
int absXLEFT = 0;
int absXRIGHT = 0;
int absYUP = 0;
int absYDOWN = 0;

int MOUSE_SENSITIVITY = 299;
int MOUSE_SENSITIVITY_NEGATIVE = -299;

// FEATURE 2: Global Cleanup Routine Triggered by OS Signals
void cleanup_system(void)
{
    printf("\n[!] Shutting down daemon. Cleaning kernel interfaces...\n");
    if (global_keyboard_fd >= 0) close(global_keyboard_fd);
    if (global_mouse_fd >= 0) close(global_mouse_fd);
    
    if (global_gamepad_fd >= 0) {
        // Destroy the virtual uinput device explicitly
        ioctl(global_gamepad_fd, UI_DEV_DESTROY);
        close(global_gamepad_fd);
    }
}

void handle_signal(int sig)
{
    keep_running = 0; 
}

void send_sync_event(int gamepad_fd)
{
    struct input_event sync_ev;
    memset(&sync_ev, 0, sizeof(struct input_event));
    sync_ev.type = EV_SYN;
    sync_ev.code = SYN_REPORT;
    sync_ev.value = 0;

    if (write(gamepad_fd, &sync_ev, sizeof(struct input_event)) < 0) {
        perror("Error writing sync event");
    }
}

void emit_gamepad_event(int gamepad_fd, uint16_t type, uint16_t code, int32_t value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(struct input_event));
    ev.type = type;
    ev.code = code;
    ev.value = value;

    if (write(gamepad_fd, &ev, sizeof(struct input_event)) < 0) {
        perror("Error writing event to virtual gamepad");
    }
}

int main(int argc, char *argv[])
{
    // FEATURE 2: Register POSIX Signal Traps
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Register safe termination cleanup exit code hook
    atexit(cleanup_system);

    char keyboard_name[256] = "Unknown";
    global_keyboard_fd = open(KEYBOARD_PATH, O_RDONLY | O_NONBLOCK);
    if (global_keyboard_fd == -1) {
        perror("Failed to open keyboard device");
        exit(EXIT_FAILURE);
    }
    ioctl(global_keyboard_fd, EVIOCGNAME(sizeof(keyboard_name)), keyboard_name);
    printf("[+] Reading Keyboard Input From: %s\n", keyboard_name);

    char mouse_name[256] = "Unknown";
    global_mouse_fd = open(MOUSE_PATH, O_RDONLY | O_NONBLOCK);
    if (global_mouse_fd == -1) {
        perror("Failed to open mouse device");
        exit(EXIT_FAILURE);
    }
    ioctl(global_mouse_fd, EVIOCGNAME(sizeof(mouse_name)), mouse_name);
    printf("[+] Reading Mouse Input From: %s\n", mouse_name);

    // Open uinput control context interface
    global_gamepad_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (global_gamepad_fd < 0) {
        perror("Failed to open /dev/uinput. Are you running as root?");
        exit(EXIT_FAILURE);
    }

    // Configure Keys
    ioctl(global_gamepad_fd, UI_SET_EVBIT, EV_KEY);
    int keys_to_set[] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, 
                         BTN_START, BTN_SELECT, BTN_THUMBL, BTN_THUMBR, 
                         BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT};
    for (size_t i = 0; i < sizeof(keys_to_set)/sizeof(int); i++) {
        ioctl(global_gamepad_fd, UI_SET_KEYBIT, keys_to_set[i]);
    }

    // Configure Absolute Joysticks
    ioctl(global_gamepad_fd, UI_SET_EVBIT, EV_ABS);
    ioctl(global_gamepad_fd, UI_SET_ABSBIT, ABS_X);
    ioctl(global_gamepad_fd, UI_SET_ABSBIT, ABS_Y);
    ioctl(global_gamepad_fd, UI_SET_ABSBIT, ABS_RX);
    ioctl(global_gamepad_fd, UI_SET_ABSBIT, ABS_RY);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Virtual Xbox Gamepad Daemon");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x3;
    uidev.id.product = 0x3;
    uidev.id.version = 2;
    
    uidev.absmax[ABS_X] = 32767;  uidev.absmin[ABS_X] = -32768; uidev.absflat[ABS_X] = 15;
    uidev.absmax[ABS_Y] = 32767;  uidev.absmin[ABS_Y] = -32768; uidev.absflat[ABS_Y] = 15;
    uidev.absmax[ABS_RX] = 512;   uidev.absmin[ABS_RX] = -512;  uidev.absflat[ABS_RX] = 16;
    uidev.absmax[ABS_RY] = 512;   uidev.absmin[ABS_RY] = -512;  uidev.absflat[ABS_RY] = 16;

    if (write(global_gamepad_fd, &uidev, sizeof(uidev)) < 0) {
        perror("Failed to define layout constraints to uinput block");
        exit(EXIT_FAILURE);
    }
    if (ioctl(global_gamepad_fd, UI_DEV_CREATE) < 0) {
        perror("Failed to instantiate uinput subsystem virtual architecture");
        exit(EXIT_FAILURE);
    }

    // FEATURE 1: Setup pollfd structure to multiplex monitoring across descriptors
    struct pollfd fds[2];
    fds[0].fd = global_keyboard_fd;
    fds[0].events = POLLIN;
    fds[1].fd = global_mouse_fd;
    fds[1].events = POLLIN;

    printf("[+] Gamepad emulation daemon listening. Press Ctrl+C or Q+Enter to stop.\n");

    struct input_event ev;

    while (keep_running) {
        // Blocks thread explicitly until kernel updates target descriptors. -1 means no timeout
        int poll_ret = poll(fds, 2, -1);
        if (poll_ret < 0) {
            if (errno == EINTR) continue; // Woken up by system call signal return handle
            perror("Poll system configuration crashed");
            break;
        }

        // Processing Keyboard Event
        if (fds[0].revents & POLLIN) {
            while (read(global_keyboard_fd, &ev, sizeof(struct input_event)) > 0) {
                if (ev.type == EV_KEY) {
                    if (ev.code == KEY_Q && ev.value == 1) {
                        q_pressed = !q_pressed;
                    }
                    if (ev.code == KEY_ENTER && ev.value == 1 && q_pressed) {
                        keep_running = 0;
                        break;
                    }
                    
                    // Key mappings
                    if (ev.code == KEY_ENTER && ev.value != 2) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_A, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_G && ev.value != 2) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_Y, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_E && ev.value != 2) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_X, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_SPACE && ev.value != 2) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_A, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_W) {
                        int val = (ev.value == 1 || ev.value == 2) ? -32768 : 0;
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_Y, val);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_S) {
                        int val = (ev.value == 1 || ev.value == 2) ? 32767 : 0;
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_Y, val);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_A) {
                        int val = (ev.value == 1 || ev.value == 2) ? -32768 : 0;
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_X, val);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_D) {
                        int val = (ev.value == 1 || ev.value == 2) ? 32767 : 0;
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_X, val);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_C && ev.value != 2) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_B, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_LEFTSHIFT) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_THUMBL, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_RIGHTSHIFT) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_THUMBR, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == KEY_LEFTCTRL) {
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RX, 0);
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RY, 0);
                        send_sync_event(global_gamepad_fd);
                    }
                }
            }
        }

        // Processing Mouse Event
        if (fds[1].revents & POLLIN) {
            while (read(global_mouse_fd, &ev, sizeof(struct input_event)) > 0) {
                if (ev.type == EV_REL) {
                    if (ev.code == REL_X) {
                        int toWrite = (ev.value > 0) ? MOUSE_SENSITIVITY : ((ev.value < 0) ? MOUSE_SENSITIVITY_NEGATIVE : 0);
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RX, toWrite);
                        send_sync_event(global_gamepad_fd);
                    }
                    if (ev.code == REL_Y) {
                        int toWrite = (ev.value > 0) ? MOUSE_SENSITIVITY : ((ev.value < 0) ? MOUSE_SENSITIVITY_NEGATIVE : 0);
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RY, toWrite);
                        send_sync_event(global_gamepad_fd);
                    }
                }
                else if (ev.type == EV_KEY) {
                    if (ev.code == BTN_LEFT) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_TL2, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == BTN_RIGHT) {
                        emit_gamepad_event(global_gamepad_fd, EV_KEY, BTN_TR2, ev.value);
                        send_sync_event(global_gamepad_fd);
                    }
                    else if (ev.code == BTN_MIDDLE) {
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RX, 0);
                        emit_gamepad_event(global_gamepad_fd, EV_ABS, ABS_RY, 0);
                        send_sync_event(global_gamepad_fd);
                    }
                }
            }
        }
    }

    return 0; // Triggering safe atexit registered hook function automatically
}
