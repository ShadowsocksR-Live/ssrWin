#define CINTERFACE
#define COBJMACROS

#include <Objbase.h>
#include <wincodec.h>
#include <Windows.h>
#include <Winerror.h>
#include <assert.h>

#pragma comment(lib, "Windowscodecs.lib")

enum image_format {
    image_format_bitmap = 0,
    image_format_png = 1,
    image_format_jpg = 2,
};

HRESULT _save_bitmap_to_file(HBITMAP bitmap, enum image_format fmt, const wchar_t* pathname);

HRESULT save_bitmap_to_bmp_file(HBITMAP bitmap, const wchar_t* pathname) {
    return _save_bitmap_to_file(bitmap, image_format_bitmap, pathname);
};

HRESULT save_bitmap_to_png_file(HBITMAP bitmap, const wchar_t* pathname) {
    return _save_bitmap_to_file(bitmap, image_format_png, pathname);
};

HRESULT save_bitmap_to_jpg_file(HBITMAP bitmap, const wchar_t* pathname) {
    return _save_bitmap_to_file(bitmap, image_format_jpg, pathname);
};

HRESULT _save_bitmap_to_file(HBITMAP bitmap, enum image_format fmt, const wchar_t* pathname)
{
    IWICImagingFactory* factory = NULL;
    IWICBitmap* wic_bitmap = NULL;
    IWICStream* stream = NULL;
    IWICBitmapEncoder* encoder = NULL;
    IWICBitmapFrameEncode* frame = NULL;
    HRESULT hr = S_OK;

    // CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // (1) Retrieve properties from the source HBITMAP.
    BITMAP bm_info = { 0 };
    if (!GetObject(bitmap, sizeof(bm_info), &bm_info)) {
        hr = E_FAIL;
    }

    // (2) Create an IWICImagingFactory instance.
    factory = NULL;
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, &factory);
    }

    // (3) Create an IWICBitmap instance from the HBITMAP.
    wic_bitmap = NULL;
    if (SUCCEEDED(hr)) {
        hr = IWICImagingFactory_CreateBitmapFromHBITMAP(factory, bitmap, NULL,
            WICBitmapIgnoreAlpha,
            &wic_bitmap);
    }

    // (4) Create an IWICStream instance, and attach it to a filename.
    stream = NULL;
    if (SUCCEEDED(hr)) {
        hr = IWICImagingFactory_CreateStream(factory, &stream);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICStream_InitializeFromFilename(stream, pathname, GENERIC_WRITE);
    }

    // (5) Create an IWICBitmapEncoder instance, and associate it with the stream.
    encoder = NULL;
    if (SUCCEEDED(hr)) {
        if (fmt == image_format_bitmap) {
            hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatBmp, NULL, &encoder);
        } else if (fmt == image_format_png) {
            hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder);
        } else if (fmt == image_format_jpg) {
            hr = IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatJpeg, NULL, &encoder);
        } else {
            assert(0);
            hr = E_FAIL;
        }
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapEncoder_Initialize(encoder, (IStream*)stream, WICBitmapEncoderNoCache);
    }

    // (6) Create an IWICBitmapFrameEncode instance, and initialize it
    // in compliance with the source HBITMAP.
    frame = NULL;
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_Initialize(frame, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_SetSize(frame, bm_info.bmWidth, bm_info.bmHeight);
    }
    if (SUCCEEDED(hr)) {
        GUID pixel_format = GUID_WICPixelFormat24bppBGR;
        hr = IWICBitmapFrameEncode_SetPixelFormat(frame, &pixel_format);
    }

    // (7) Write bitmap data to the frame.
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_WriteSource(frame, (IWICBitmapSource*)wic_bitmap, NULL);
    }

    // (8) Commit frame and data to stream.
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapFrameEncode_Commit(frame);
    }
    if (SUCCEEDED(hr)) {
        hr = IWICBitmapEncoder_Commit(encoder);
    }

    // Cleanup
    if (frame) {
        IWICBitmapFrameEncode_Release(frame);
    }
    if (encoder) {
        IWICBitmapEncoder_Release(encoder);
    }
    if (stream) {
        IWICStream_Release(stream);
    }
    if (wic_bitmap) {
        IWICBitmap_Release(wic_bitmap);
    }
    if (factory) {
        IWICImagingFactory_Release(factory);
    }

    return hr;
}
