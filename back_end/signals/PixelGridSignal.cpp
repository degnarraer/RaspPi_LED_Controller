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
      signal_(SignalManager::getInstance().createSignal<std::vector<std::vector<RGB>>>( signalName_,
                                                                                        webSocketServer_,
                                                                                        get_rgb_matrix_to_binary_encoder()))
{
    logger_ = initializeLogger("PixelGridSignal", spdlog::level::info);
    logger_->info("PixelGridSignal created with dimensions: {}x{}", width_, height_);
    ledController_->run();
    ledController_->setDeviceGlobalBrightness(5);
}

void PixelGridSignal::setPixel(size_t x, size_t y, RGB color)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    ledController_->setPixel(y, color.r, color.g, color.b, 1.0);
    if (x < width_ && y < height_)
    {
        pixels_[y][x] = color;
    }
}

RGB PixelGridSignal::getValue(size_t x, size_t y) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (x < width_ && y < height_)
    {
        return pixels_[y][x];
    }
    return {0, 0, 0};
}

void PixelGridSignal::clear(RGB color)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& row : pixels_)
    {
        std::fill(row.begin(), row.end(), color);
    }
}

void PixelGridSignal::notify()
{
    std::lock_guard<std::mutex> lock(mutex_);
    signal_->setValue(pixels_);
}

std::shared_ptr<Signal<std::vector<std::vector<RGB>>>> PixelGridSignal::GetSignal() const
{
    return signal_;
}
