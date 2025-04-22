#include "PixelGridSignal.h"

PixelGridSignal::PixelGridSignal( const std::string& baseSignalName
                                , size_t width
                                , size_t height
                                , std::shared_ptr<WebSocketServer> webSocketServer )
                                : baseName_(baseSignalName)
                                , width_(width)
                                , height_(height)
                                , webSocketServer_(webSocketServer)
{
    rowSignals_.reserve(height_);
    for (size_t row = 0; row < height_; ++row)
    {
        auto signal = SignalManager::GetInstance().CreateSignal<std::vector<RGB>>(
            baseSignalName + " Row " + std::to_string(row),
            webSocketServer_,
            static_cast<std::string(*)(const std::string&, const std::vector<RGB>&)>(encode_signal_name_and_value));
        signal->SetValue(std::vector<RGB>(width_, {0, 0, 0}));
        rowSignals_.push_back(signal);
    }
}

void PixelGridSignal::SetPixel(size_t x, size_t y, RGB color)
{
    if (x >= width_ || y >= height_) return;

    auto& row = rowSignals_[y];
    auto rowData = row->GetData();
    if ((*rowData)[x] != color)
    {
        (*rowData)[x] = color;
    }
}

RGB PixelGridSignal::GetPixel(size_t x, size_t y) const
{
    if (x >= width_ || y >= height_) return {0, 0, 0};

    auto rowData = rowSignals_[y]->GetData();
    return (*rowData)[x];
}

void PixelGridSignal::Clear(RGB color)
{
    for (size_t y = 0; y < height_; ++y)
    {
        auto& row = rowSignals_[y];
        auto rowData = row->GetData();
        bool changed = false;
        for (auto& pixel : *rowData)
        {
            if (pixel != color)
            {
                pixel = color;
                changed = true;
            }
        }
    }
}

void PixelGridSignal::Notify()
{
    for (size_t row = 0; row < height_; ++row)
    {
        rowSignals_[row]->Notify();
    }
}

void PixelGridSignal::Notify(size_t row)
{
    if (row < height_)
    {
        rowSignals_[row]->Notify();
    }
}

std::shared_ptr<Signal<std::vector<RGB>>> PixelGridSignal::GetRowSignal(size_t row) const
{
    if (row < height_)
    {
        return rowSignals_[row];
    }
    return nullptr;
}
