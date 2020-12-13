#include <windows.h>
#include <tchar.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <qrencode.h>

#include "qrcode_gen.h"

#define BI_RGB          0L
#define IMG_MAX_WIDTH   400

BOOL generate_qr_code_image_file(const char* szSourceSring, const char* szFilePath)
{
    HBITMAP hBmp = NULL;
    unsigned int unWidth, x, y, l, n, step, unWidthAdjusted, unDataBytes;
    unsigned char* pRGBData, * pSourceData, * pDestData;
    QRcode* pQRC;
    FILE* f;

    if (szSourceSring == NULL || szFilePath == NULL) {
        assert(!"szSourceSring == NULL || szFilePath == NULL");
        return FALSE;
    }

    // Compute QRCode
    if ((pQRC = QRcode_encodeString(szSourceSring, 0, QR_ECLEVEL_H, QR_MODE_8, 1)) == NULL) {
        assert(!"QRcode_encodeString failed\n");
        return FALSE;
    }

    {
        BITMAPFILEHEADER kFileHeader = { 0 };
        BITMAPINFOHEADER kInfoHeader = { 0 };
        int scale_rate = IMG_MAX_WIDTH / pQRC->width;
        if (scale_rate < 1) {
            scale_rate = 1;
        }

        unWidth = pQRC->width;
        unWidthAdjusted = unWidth * scale_rate * 3;
        if (unWidthAdjusted % sizeof(UINT32)) {
            unWidthAdjusted = (unWidthAdjusted / sizeof(UINT32) + 1) * sizeof(UINT32);
        }
        unDataBytes = unWidthAdjusted * unWidth * scale_rate;
        // Allocate pixels buffer
        if (!(pRGBData = (unsigned char*)malloc(unDataBytes))) {
            assert(!"Out of memory");
            return FALSE;
        }

        step = pQRC->width;
        for (x = 0; x < step / 2; ++x) {
            memcpy(pRGBData, pQRC->data + x * step, step);
            memcpy(pQRC->data + x * step, pQRC->data + (step - 1 - x) * step, step);
            memcpy(pQRC->data + (step - 1 - x) * step, pRGBData, step);
        }

        // Preset to white
        memset(pRGBData, 0xff, unDataBytes);
        // Prepare bmp headers
        kFileHeader.bfType = 0x4d42;  // "BM"
        kFileHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + unDataBytes;
        kFileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        kInfoHeader.biSize = sizeof(BITMAPINFOHEADER);
        kInfoHeader.biWidth = unWidth * scale_rate;
        kInfoHeader.biHeight = ((int)unWidth * scale_rate);
        kInfoHeader.biCompression = BI_RGB;
        kInfoHeader.biPlanes = 1;
        kInfoHeader.biBitCount = 24;

        // Convert QrCode bits to bmp pixels
        pSourceData = pQRC->data;
        for (y = 0; y < unWidth; y++) {
            pDestData = pRGBData + unWidthAdjusted * y * scale_rate;
            for (x = 0; x < unWidth; x++) {
                if (*pSourceData & 0x01) {
                    for (l = 0; l < (unsigned int)scale_rate; l++) {
                        for (n = 0; n < (unsigned int)scale_rate; n++) {
                            *(pDestData + 0 + n * 3 + unWidthAdjusted * l) = 0;
                            *(pDestData + 1 + n * 3 + unWidthAdjusted * l) = 0;
                            *(pDestData + 2 + n * 3 + unWidthAdjusted * l) = 0;
                        }
                    }
                }
                pDestData += 3 * scale_rate;
                pSourceData++;
            }
        }

        // Output the bmp file
        if (!(fopen_s(&f, szFilePath, "wb"))) {
            fwrite(&kFileHeader, sizeof(BITMAPFILEHEADER), 1, f);
            fwrite(&kInfoHeader, sizeof(BITMAPINFOHEADER), 1, f);
            fwrite(pRGBData, sizeof(unsigned char), unDataBytes, f);
            fclose(f);
        }
        else {
            assert(!"Unable to open file\n");
            return FALSE;
        }

        free(pRGBData);
        QRcode_free(pQRC);
    }
    return TRUE;
}

HBITMAP generate_qr_code_image(const char* szSourceSring)
{
    HBITMAP hBmp = NULL;
    unsigned int unWidth, x, y, l, n, unWidthAdjusted, unDataBytes;
    unsigned char* pRGBData, * pSourceData, * pDestData;
    QRcode* pQRC;

    // Compute QRCode
    if ((pQRC = QRcode_encodeString(szSourceSring, 0, QR_ECLEVEL_H, QR_MODE_8, 1)) == NULL) {
        assert(!"QRcode_encodeString failed\n");
        return NULL;
    }

    {
        int scale_rate = IMG_MAX_WIDTH / pQRC->width;
        if (scale_rate < 1) {
            scale_rate = 1;
        }

        unWidth = pQRC->width;
        unWidthAdjusted = unWidth * scale_rate * sizeof(UINT32);
        unDataBytes = unWidthAdjusted * unWidth * scale_rate;
        // Allocate pixels buffer
        if (!(pRGBData = (unsigned char*)malloc(unDataBytes))) {
            assert(!"Out of memory");
            return NULL;
        }

        // Preset to white
        memset(pRGBData, 0xff, unDataBytes);
        pSourceData = pQRC->data;
        unWidthAdjusted = unWidth * scale_rate * sizeof(UINT32);
        for (y = 0; y < unWidth; y++) {
            pDestData = pRGBData + unWidthAdjusted * y * scale_rate;
            for (x = 0; x < unWidth; x++) {
                if (*pSourceData & 0x01) {
                    for (l = 0; l < (unsigned int)scale_rate; l++) {
                        for (n = 0; n < (unsigned int)scale_rate; n++) {
                            *((UINT32*)(pDestData + 0 + n * sizeof(UINT32) + unWidthAdjusted * l)) = 0;
                        }
                    }
                }
                pDestData += sizeof(UINT32) * scale_rate;
                pSourceData++;
            }
        }

        hBmp = CreateBitmap(pQRC->width * scale_rate, pQRC->width * scale_rate, 1, 32, pRGBData);

        free(pRGBData);
        QRcode_free(pQRC);
    }
    return hBmp;
}

HBITMAP generate_qr_code_image_2(const char* szSourceSring)
{
    char tmp_path[MAX_PATH] = { 0 }, tmp_file[MAX_PATH] = { 0 };
    GetTempPathA(ARRAYSIZE(tmp_path), tmp_path);
    GetTempFileNameA(tmp_path, "ssr_", 0, tmp_file);
    generate_qr_code_image_file(szSourceSring, tmp_file);
    return (HBITMAP)LoadImageA(NULL, tmp_file, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}
