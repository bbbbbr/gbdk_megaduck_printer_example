#include <gbdk/platform.h>
#include <gbdk/console.h>
#include <gb/isr.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <duck/laptop_io.h>

#include "megaduck_printer.h"

bool    printer_init_result;
uint8_t printer_type;

const uint8_t bg_tile[] = {
    0x00, 0x00,
    0x00, 0x00,
    0xFF, 0x00,
    0xFF, 0x00,
    0x00, 0xFF,
    0x00, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF, };

uint8_t printer_query(void) {

     printf("Printer Query\n");

    uint8_t printer_query_result = duck_io_printer_query();
    printf("Printer Result %hx\n", printer_query_result);

    // return (printer_query_result & DUCK_IO_PRINTER_INIT_OK);
    return printer_query_result;
}


static bool duck_laptop_and_printer_init(void) {

    bool megaduck_laptop_init_ok = duck_io_laptop_init();
    if (!megaduck_laptop_init_ok) {
        // If laptop hardware is not present then there isn't anything
        // for this program to do, so just idle in a loop
        printf("Laptop not detected\n"
               "or Failed to Initialize\n");
        return false;
    }

    // Otherwise laptop init succeeded
    printf("Laptop Detected!\n");


// FIXME: Printer does not seem to detect on startup?

    // // // Check to see if the printer is connected, and if so what model
    // // printer_init_result = duck_io_printer_detected();

    // // if (printer_init_result == false) {
    // //     // If laptop hardware is not present then there isn't anything
    // //     // for this program to do, so just idle in a loop
    // //     printf("Printer not detected, trying again..\n");

    // //     if (printer_query() == false)
    // //         return false;
    // // }

    // // Otherwise printer init succeeded
    // printf("Printer Detected!\n");
    // if (duck_io_printer_type() == DUCK_IO_PRINTER_TYPE_1_PASS)
    //     printf("- Single Pass model\n");
    // else
    //     printf("- Double Pass model\n");

    return true;    
}


bool duck_io_launch_cart(void) {

    printf("Launch Cart Cmd\n");
    uint8_t retry_count = 5;
    bool    command_status = false;

    while (retry_count--) {
        duck_io_send_byte(DUCK_IO_CMD_RUN_CART_IN_SLOT);
        if (duck_io_read_byte_with_msecs_timeout(DUCK_IO_TIMEOUT_200_MSEC)) {
            command_status = true;
            break;
        }
    }

    printf("Result: %hx\n", command_status);
    // Fail if no reply to the command arrived
    if (command_status == false) {
        return false;
    }

    // Fail if no cart was found in the slot
    if (duck_io_rx_byte == DUCK_IO_REPLY_NO_CART_IN_SLOT) {
        return false;
    }
    return true;
}

extern void test_single_send(void);

void main(void) {


    uint8_t gamepad = 0;
    uint8_t gamepad_last = 0;

    SPRITES_8x8;
    SHOW_SPRITES;
    SHOW_BKG;
    printf("Initializing..\n");

    // Set tile as last tile
    set_bkg_data(255, 1, bg_tile);    
    fill_bkg_rect(0, DEVICE_SCREEN_HEIGHT - 4, DEVICE_SCREEN_WIDTH - 1, DEVICE_SCREEN_HEIGHT - 1, 255);

    // Stop here if no printer detected
    if (duck_laptop_and_printer_init() == false) {
        // Optionally take action if no printer is detected
    }

    printf("\n* Press START\n to print screen\n");

	while(1) {
	    vsync();
        gamepad_last = gamepad;
        gamepad = joypad();

        // Send command to print the screen if START is pressed
        switch (gamepad) {
            case J_START:
                printf("Starting print...\n");
                uint8_t printer_status = printer_query();
                bool print_job_status = duck_io_print_screen(printer_status);
                printf("Finished print, status: %hu\n", print_job_status);
                // Wait until START is released before continuing
                waitpadup();
                break;
            case J_UP:    SCY_REG--; break;
            case J_DOWN:  SCY_REG++; break;
            case J_LEFT:  SCX_REG--; break;
            case J_RIGHT: SCX_REG++; break;
            case J_SELECT: printer_query(); waitpadup(); break;
            case J_B: test_single_send(); waitpadup(); break;
            case J_A: printf("tx: %hx\n", SCX_REG); duck_io_send_byte(SCX_REG); printf("done\n");
                        waitpadup(); break;
        }
	}
}
