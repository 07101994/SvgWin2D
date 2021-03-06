#include "pch.h"

#include "svg.h"

using namespace Microsoft::Graphics::Canvas;
using namespace Microsoft::Graphics::Canvas::Geometry;
using namespace Microsoft::Graphics::Canvas::Text;
using namespace Windows::Foundation;
using namespace Windows::UI;


void element::draw(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    s->push();
    s->current()->set(
        color_,
        fillPaint_,
        strokePaint_,
        strokeWidth_,
        fontFamily_,
        fontSize_);

    auto oldTransform = ds->Transform;
    if (transform_)
        ds->Transform = *transform_ * ds->Transform;

    draw_element(ds, destinationSize, s);

    ds->Transform = oldTransform;
    s->pop();
}


void container_element::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    for (auto const& child : elements_)
    {
        child->draw(ds, destinationSize, s);
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

    auto width = calculate_width_or_height(width_, destinationSize.Width);
    auto height = calculate_width_or_height(height_, destinationSize.Height);

    draw_element(ds, float2{ width, height}, std::make_unique<inherited_style>().get());

    delete ds;

    return content;
}


float3x2 make_transform(float2 destinationSize, viewBox const& viewBox, preserveAspectRatio aspectRatio)
{
    using namespace Windows::Foundation::Numerics;
    float scaleX = destinationSize.x / viewBox.Width;
    float scaleY = destinationSize.y / viewBox.Height;
    return make_float3x2_translation(-viewBox.X, -viewBox.Y)
        * make_float3x2_scale(scaleX, scaleY);
}


void svg::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto originalTransform = ds->Transform;

    if (viewBox_)
    {
        ds->Transform = make_transform(destinationSize, *viewBox_.get(), preserveAspectRatio_);
    }

    container_element::draw_element(ds, destinationSize, s);

    ds->Transform = originalTransform;
}


void circle::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto fb = s->current()->fillBrush(ds);
    if (fb)
        ds->FillCircle(cx_.Number, cy_.Number, radius_.Number, fb);

    auto sb = s->current()->strokeBrush(ds);
    if (sb)
        ds->DrawCircle(cx_.Number, cy_.Number, radius_.Number, sb, s->current()->stroke_width());
}


void ellipse::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto fb = s->current()->fillBrush(ds);
    if (fb)
        ds->FillEllipse(cx_.Number, cy_.Number, rx_.Number, ry_.Number, fb);

    auto sb = s->current()->strokeBrush(ds);
    if (sb)
        ds->DrawEllipse(cx_.Number, cy_.Number, rx_.Number, ry_.Number, sb, s->current()->stroke_width());
}


void rect::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    Rect rect{ x_.Number, y_.Number, width_.Number, height_.Number };

    auto fb = s->current()->fillBrush(ds);
    if (fb)
    {
        if (rx_ && ry_)
            ds->FillRoundedRectangle(rect, rx_->Number, ry_->Number, fb);
        else if (rx_)
            ds->FillRoundedRectangle(rect, rx_->Number, rx_->Number, fb);
        else if (ry_)
            ds->FillRoundedRectangle(rect, ry_->Number, ry_->Number, fb);
        else
            ds->FillRectangle(rect, fb);
    }

    auto sb = s->current()->strokeBrush(ds);
    if (sb)
    {
        auto strokeWidth = s->current()->stroke_width();

        if (rx_ && ry_)
            ds->DrawRoundedRectangle(rect, rx_->Number, ry_->Number, sb, strokeWidth);
        else if (rx_)
            ds->DrawRoundedRectangle(rect, rx_->Number, rx_->Number, sb, strokeWidth);
        else if (ry_)
            ds->DrawRoundedRectangle(rect, ry_->Number, ry_->Number, sb, strokeWidth);
        else
            ds->DrawRectangle(rect, sb, strokeWidth);
    }
}


void line::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto sb = s->current()->strokeBrush(ds);
    if (sb)
    {
        auto strokeWidth = s->current()->stroke_width();

        ds->DrawLine(x1_.Number, y1_.Number, x2_.Number, y2_.Number, sb, strokeWidth);
    }
}


void polything::draw_polything(CanvasDrawingSession^ ds, inherited_style* s, CanvasFigureLoop loop)
{
    // TODO: now that path builds the geometry up during parsing, maybe polything should too?

    auto path = ref new CanvasPathBuilder(ds);

    bool first = true;

    for (auto const& point : points_)
    {
        if (first)
        {
            path->BeginFigure(point.first, point.second);
            first = false;
        }
        else
        {
            path->AddLine(point.first, point.second);
        }
    }

    path->EndFigure(loop);

    auto geometry = CanvasGeometry::CreatePath(path);

    auto fb = s->current()->fillBrush(ds);
    if (fb)
    {
        ds->FillGeometry(geometry, fb);
    }

    auto sb = s->current()->strokeBrush(ds);
    if (sb)
    {
        auto strokeWidth = s->current()->stroke_width();
        ds->DrawGeometry(geometry, sb, strokeWidth);
    }
}


void polyline::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    draw_polything(ds, s, CanvasFigureLoop::Open);
}


void polygon::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    draw_polything(ds, s, CanvasFigureLoop::Closed);
}


void path::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto fb = s->current()->fillBrush(ds);
    if (fb)
    {
        ds->FillGeometry(geometry_, fb);
    }

    auto sb = s->current()->strokeBrush(ds);
    if (sb)
    {
        auto strokeWidth = s->current()->stroke_width();
        auto strokeStyle = ref new CanvasStrokeStyle();
        strokeStyle->LineJoin = CanvasLineJoin::MiterOrBevel;
        ds->DrawGeometry(geometry_, sb, strokeWidth, strokeStyle);
    }
}


void text::draw_element(CanvasDrawingSession^ ds, float2 destinationSize, inherited_style* s)
{
    auto fb = s->current()->fillBrush(ds);
    if (fb)
    {
        ds->DrawText(text_, x_.Number, y_.Number, fb, s->current()->text_format());
    }
}