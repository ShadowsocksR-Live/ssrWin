#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__cplusplus)
#define inline __inline
#endif
#include <zbar.h>

#include "qrcode_dec.h"
#include "save_bitmap.h"

char* qr_code_decoder(HBITMAP hBmp, void* (*allocator)(size_t)) {
    char* result = NULL;
    int width, height, n;
    unsigned char* raw = NULL;
    zbar_image_t *image;
    const zbar_symbol_t *symbol;
    zbar_image_scanner_t *scanner = NULL;

    if (hBmp == NULL || allocator == NULL) {
        return NULL;
    }

    /* obtain image data */
    width = 0, height = 0;
    raw = NULL;
    extract_bitmap_in_grayscale_8bpp(hBmp, &width, &height, &raw);
    if (raw == NULL) {
        return result;
    }

    /* create a reader */
    scanner = zbar_image_scanner_create();

    /* configure the reader */
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);

    /* wrap image data */
    image = zbar_image_create();
    zbar_image_set_format(image, zbar_fourcc('Y','8','0','0'));
    zbar_image_set_size(image, width, height);
    zbar_image_set_data(image, raw, width * height, zbar_image_free_data);

    /* scan the image for barcodes */
    n = zbar_scan_image(scanner, image);

    /* extract results */
    symbol = zbar_image_first_symbol(image);
    for(; symbol; symbol = zbar_symbol_next(symbol)) {
        /* do something useful with results */
        zbar_symbol_type_t typ = zbar_symbol_get_type(symbol);
        const char *data = zbar_symbol_get_data(symbol);
        // printf("decoded %s symbol \"%s\"\n", zbar_get_symbol_name(typ), data);
        result = (char*)allocator(strlen(data) + 1);
        if (result == NULL) {
            break;
        }
        strcpy(result, data);
        break;
    }

    /* clean up */
    zbar_image_destroy(image);
    zbar_image_scanner_destroy(scanner);

    return result;
}
