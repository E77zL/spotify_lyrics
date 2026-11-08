// MIT. A GPU-composed backdrop behind the existing crisp layered foreground.
#pragma once
#include "NativeDrawing.h"
#include <memory>
void ApplyHudFrame(HWND window);
class NativeGlass
{
    struct State;
    std::unique_ptr<State> state;
    bool attempted = false;
    HRESULT failure = S_OK;

  public:
    NativeGlass();
    ~NativeGlass();
    NativeGlass(const NativeGlass &) = delete;
    NativeGlass &operator=(const NativeGlass &) = delete;
    void attach(HWND foreground);
    void sync(int radius, int opacity, double fade = 1);
    void release();
    HWND window() const;
    double strength() const;
    HRESULT error() const
    {
        return failure;
    }
};
