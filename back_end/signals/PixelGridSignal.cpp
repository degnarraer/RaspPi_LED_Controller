#include "PixelGridSignal.h"

PixelGridSignal::PixelGridSignal(const std::string& signalName,
                                 size_t width,
                                 size_t height,
                                 std::shared_ptr<WebSocketServer> webSocketServer)
    : signalName_(signalName),
      width_(width),
      height_(height),
      webSocketServer_(std::move(webSocketServer)),
      pixels_(height, std::vector<RGB>(width, RGB{0, 0, 0})),
      signal_(SignalManager::GetInstance().CreateSignal<std::vector<std::vector<RGB>>>( signalName_,
                                                                                        webSocketServer_,
                                                                                        get_rgb_matrix_to_binary_encoder()))
{
    logger_ = InitializeLogger("PixelGridSignal", spdlog::level::info);
    logger_->info("PixelGridSignal created with dimensions: {}x{}", width_, height_);
}

void PixelGridSignal::SetPixel(size_t x, size_t y, RGB color)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (x < width_ && y < height_)
    {
        pixels_[y][x] = color;
    }
}

RGB PixelGridSignal::GetPixel(size_t x, size_t y) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (x < width_ && y < height_)
    {
        return pixels_[y][x];
    }
    return {0, 0, 0};
}

void PixelGridSignal::Clear(RGB color)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& row : pixels_)
    {
        std::fill(row.begin(), row.end(), color);
    }
}

void PixelGridSignal::Notify()
{
    std::lock_guard<std::mutex> lock(mutex_);
    signal_->SetValue(pixels_);
}

std::shared_ptr<Signal<std::vector<std::vector<RGB>>>> PixelGridSignal::GetSignal() const
{
    return signal_;
}
