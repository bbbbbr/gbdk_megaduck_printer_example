#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#include <duck/laptop_io.h>
#include "megaduck_printer.h"

static void prepare_tile_row(uint8_t row, uint8_t tile_bitplane_offset);
static void convert_tile (uint8_t * p_out_buf, uint8_t * p_tile_buf);
static bool duck_io_send_tile_row_1pass(void);
static bool duck_io_send_tile_row_2pass(uint8_t tile_bitplane_offset);

#define BYTES_PER_PRINTER_TILE  8u
#define BYTES_PER_VRAM_TILE     16u
#define TILE_HEIGHT             8u
#define TILE_WIDTH              8u

// Not yet sure if it *must* be buffered and delivered at constant speed or
// if the print data packets can be delivered at variable speed
uint8_t tile_row_buffer[DEVICE_SCREEN_WIDTH * BYTES_PER_PRINTER_TILE];


void test_single_send(void) {
    uint8_t map_row = 0x04;

    prepare_tile_row(map_row, BITPLANE_0);
    duck_io_send_tile_row_2pass(BITPLANE_0);
}

// Currently unknown:
// - Single pass printer probably does not support variable image width
// - Double pass printer might, since it has explicit Carriage Return and Line Feed commands, but it's not verified
//
// So for the time being require full screen image width
bool duck_io_print_screen(void) {

    for (uint8_t map_row = 0; map_row < DEVICE_SCREEN_HEIGHT; map_row++) {

        if (duck_io_printer_type() == DUCK_IO_PRINTER_TYPE_2_PASS) {
            // First bitplane, fail out if there was a problem
            prepare_tile_row(map_row, BITPLANE_0);
            if (duck_io_send_tile_row_2pass(BITPLANE_0) == false) return false;
            // Second bitplane
            prepare_tile_row(map_row, BITPLANE_1);
            if (duck_io_send_tile_row_2pass(BITPLANE_1) == false) return false;
        } else {
            // First bitplane, fail out if there was a problem
            prepare_tile_row(map_row, BITPLANE_0);
            if (duck_io_send_tile_row_1pass() == false) return false;
        }
    }
    return true;
}


static void prepare_tile_row(uint8_t row, uint8_t tile_bitplane_offset) {
    
    uint8_t tile_buffer[BYTES_PER_VRAM_TILE];
    uint8_t * p_row_buffer = tile_row_buffer;

    // Add Scroll offset to closest tile
    row        += (SCY_REG / TILE_HEIGHT);
    uint8_t col = (SCX_REG / TILE_WIDTH);

    // Loop through tile columns for the current tile row
    for (uint8_t tile = 0u; tile < DEVICE_SCREEN_WIDTH; tile++) {
        
        // Get the Tile ID from the map then copy it's tile pattern data into a buffer
        uint8_t tile_id = get_bkg_tile_xy((col + tile) & (DEVICE_SCREEN_BUFFER_HEIGHT - 1), 
                                          row & (DEVICE_SCREEN_BUFFER_HEIGHT - 1));
        get_bkg_data(tile_id, 1u, tile_buffer);

        // Mirror, Rotate -90 degrees and reduce tile to 1bpp
        convert_tile(p_row_buffer, tile_buffer + tile_bitplane_offset);
        p_row_buffer += BYTES_PER_PRINTER_TILE;
    }
}


    // for (uint8_t t=0u; t < 160; t++) {
    //     // This (1bpp) input tile
    //     //
    //     //       bits
    //     //      7 ___ 0
    //     //     x........  = 0x80
    //     //     x........  = 0x80
    //     //     x........  = 0x80
    //     // b 7 x........  = 0x80
    //     // y . .........  = 0x00
    //     // t . .........  = 0x00
    //     // e 0 .........  = 0x00
    //     //     .xxxxxxxx  = 0x7F
    //     //
    //     // Results in these bytes sent:
    //     //   0   0xF0
    //     //   1   0x01
    //     //   2   0x01
    //     //   3   0x01
    //     //   4   0x01
    //     //   5   0x01
    //     //   6   0x01
    //     //   7   0x01
    //     //      
    //     // Results in the following output:
    //     //
    //     //       bytes
    //     //      0 ___ 7
    //     //     x........
    //     //     x........
    //     //     x........
    //     // b 7 x........
    //     // i | .........
    //     // t | .........
    //     // s 0 .........
    //     //     .xxxxxxxx
    //     //
    //     if ((t & 0x07u) == 0u) tile_row_buffer[t] = 0xF0u;
    //     else tile_row_buffer[t] = 0x01u;
    // }


// For printing tile needs to be flipped horizontally and rotated -90 degrees
//
// Normal tile:    Printer tile: (Note changed axes)
//    bits           bytes
// b  --X--          --X--
// y |  7 .. 0    b |   0 .. 7
// t Y 0          i Y 7        
// e | .          t | . 
// s | 7          s | 0     
//
static void convert_tile (uint8_t * p_out_buf, uint8_t * p_tile_buf) {

    // Clear printer tile
    for (uint8_t c = 0u; c < BYTES_PER_PRINTER_TILE; c++) {
        p_out_buf[c] = 0u;
    }

    // Transform tile bytes into printable row buffer bytes
    // Tile must get flipped horizontally and rotated -90 degrees
    // 
    // Note the +2 increment for tile row, skipping the interleaved higher bit plane
    //
    // For each tile row byte take the X axis bits representing pixels
    // and transform them into column oriented bits spread across 8 bytes
    //
    uint8_t out_bit = 0x80u;  // X axis bit to set in the output for corresponding input
    for (uint8_t vram_tile_row = 0u; vram_tile_row < BYTES_PER_VRAM_TILE; vram_tile_row += 2u) {
        uint8_t tile_byte = p_tile_buf[vram_tile_row];

        // Scan X axis Left to right
        uint8_t tile_bit = 0x80u;
        for (uint8_t out_col = 0; out_col < TILE_HEIGHT; out_col++) {

            if (tile_byte & tile_bit) p_out_buf[out_col] |= out_bit;
            tile_bit >>= 1;
        }
        out_bit >>= 1;
    }
}


// TODO can the last packet be merged in with just a bunch of conditionals?
static bool duck_io_send_tile_row_2pass(uint8_t tile_bitplane_offset) {

    uint8_t * p_row_buffer = tile_row_buffer;
    uint8_t   reserved_packet_end_bytes = 0;  // 0 for all packets except the last two;

    // Send 13 x 12 byte packets with row data
    duck_io_tx_buf_len = PRINTER_LEN_12_ROW_DATA;
    for (uint8_t packet = 0u; packet < PRINTER_2_PASS_ROW_NUM_PACKETS; packet++) {

        // If it's the last packet, change to row terminator packet style
        if (packet == PRINTER_2_PASS_ROW_LAST_PACKET) {
            // Insert CR command at/near end of packet
            duck_io_tx_buf[PRINTER_CR_IDX] = PRINTER_CARRIAGE_RETURN;
    
                // First bitplane pass is Carriage return only
            if (tile_bitplane_offset == 0) {
                duck_io_tx_buf_len = PRINTER_LEN_5_END_ROW_CR;
            } else {
                // Second bit plane is last for row, so insert LF command at end of packet
                duck_io_tx_buf_len = PRINTER_LEN_6_END_ROW_CRLF;
                duck_io_tx_buf[PRINTER_LF_IDX] = PRINTER_LINE_FEED;
            }
            // Terminator packet always has 4 data bytes, remaining ones are control chars set above
            reserved_packet_end_bytes = duck_io_tx_buf_len - PRINTER_LEN_END_ROW_DATA_SZ;
        }

        for (uint8_t c = 0u; c < (duck_io_tx_buf_len - reserved_packet_end_bytes); c++)
            duck_io_tx_buf[c] = *p_row_buffer++;

        if (!duck_io_send_cmd_and_buffer(DUCK_IO_CMD_PRINT_SEND_BYTES))
            return false; // Fail out if there was a problem
    }

    // End of row: wait for Carriage Return confirmation ACK from the printer
    // System ROM doesn't seem to care about the return value, so we won't either for now
    duck_io_read_byte_with_msecs_timeout(PRINT_ROW_END_ACK_WAIT_TIMEOUT_200MSEC);

    return true; // Success
}


// TODO can the last packet be merged in with just a bunch of conditionals?
static bool duck_io_send_tile_row_1pass(void) {

    uint8_t * p_row_buffer = tile_row_buffer;

    // Send 13 x 12 byte packets with row data
    duck_io_tx_buf_len = PRINTER_LEN_12_ROW_DATA;
    for (uint8_t packet = 0u; packet < PRINTER_1_PASS_ROW_NUM_PACKETS; packet++) {

        for (uint8_t c = 0u; c < (duck_io_tx_buf_len); c++)
            duck_io_tx_buf[c] = *p_row_buffer++;

        if (!duck_io_send_cmd_and_buffer(DUCK_IO_CMD_PRINT_SEND_BYTES))
            return false; // Fail out if there was a problem
    }

    uint8_t txbyte;
    // Now send remaining bulk non-packetized data (unclear why transmit methods are split)
    for (txbyte = 0u; txbyte < PRINTER_1_PASS_ROW_NUM_BULK_DATA_BYTES; txbyte++) {
        duck_io_read_byte_with_msecs_timeout(PRINTER_1_PASS_BULK_ACK_TIMEOUT_100MSEC);
        duck_io_send_byte(*p_row_buffer++);
    }

    // Send last four bulk bytes after end of tile data, unclear what they are for
    for (txbyte = 0u; txbyte < PRINTER_1_PASS_ROW_NUM_BULK_UNKNOWN_BYTES; txbyte++) {
        duck_io_read_byte_with_msecs_timeout(PRINTER_1_PASS_BULK_ACK_TIMEOUT_100MSEC);
        duck_io_send_byte(0x00);
    }

    // Wait for last bulk data ACK (with 1msec delay for unknown reason)
    delay(1);
    duck_io_read_byte_with_msecs_timeout(PRINT_ROW_END_ACK_WAIT_TIMEOUT_200MSEC);
    
    // End of row: wait for Carriage Return confirmation ACK from the printer
    // System ROM doesn't seem to care about the return value, so we won't either for now
    duck_io_read_byte_with_msecs_timeout(PRINT_ROW_END_ACK_WAIT_TIMEOUT_200MSEC);

    return true; // Success
}
