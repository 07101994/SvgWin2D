#include "pch.h"

#include "svg.h"

using namespace Microsoft::Graphics::Canvas;
using namespace Windows::Foundation;
using namespace Windows::UI;

void container_element::add_child(std::unique_ptr<element>&& child)
{
    elements_.push_back(std::move(child));
}


void container_element::draw(CanvasDrawingSession^ ds)
{
    for (auto const& child : elements_)
    {
        child->draw(ds);
    }
}


static float calculate_width_or_height(length svgLength, float destinationLength)
{
    switch (svgLength.Unit)
    {
    case unit::percent:
        return destinationLength * std::max(0.0f, std::min(100.0f, svgLength.Number)) / 100.0f;

    case unit::px:
        return svgLength.Number;

    default:
        return svgLength.Number; // TODO: all the other units!
    }
}


ICanvasImage^ svg::create_image(ICanvasResourceCreator^ resourceCreator, Size destinationSize)
{
    auto content = ref new CanvasCommandList(resourceCreator);
    auto ds = content->CreateDrawingSession();
    ds->Clear(Colors::Transparent);
    draw(ds);
    delete ds;

    auto crop = ref new Effects::CropEffect();

    auto width = calculate_width_or_height(width_, destinationSize.Width);
    auto height = calculate_width_or_height(height_, destinationSize.Height);

    crop->SourceRectangle = Rect{ 0, 0, width, height };
    crop->Source = content;

    return crop;
}


void circle::draw(CanvasDrawingSession^ ds)
{
    ds->DrawCircle(cx_.Number, cy_.Number, radius_.Number, Colors::Black);
}