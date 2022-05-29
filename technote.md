# HWND からスクリーンキャプチャする
Win32 から WinRT Windows.Graphics.Capture を呼ぶ。
Native DirectX11 と WinRT と中間レイヤが混ざってカオス。
https://github.com/microsoft/Windows.UI.Composition-Win32-Samples/tree/master/cpp/ScreenCaptureforHWND

# テクスチャからピクセルデータを読む
CPU Read オプションで CPU から読めるようメモリマップして読む。
ダメだったら同じ大きさのテクスチャを用意してコピーしてからマップする。
https://github.com/Microsoft/graphics-driver-samples/blob/master/render-only-sample/rostest/util.cpp#L244
