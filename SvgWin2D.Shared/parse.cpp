#include "pch.h"

#include "parse.h"

using namespace Windows::Data::Xml::Dom;

Platform::String^ SVG_NS = L"http://www.w3.org/2000/svg";
Platform::String^ XMLNS_SVG = L"xmlns:svg='http://www.w3.org/2000/svg'";

std::wregex gTokenRegex(L"(-?[0-9.]+)\\s*,?\\s*");

class list_parser
{
    Platform::String^ toParse_;
    std::regex_token_iterator<const wchar_t*> it_;
    
public:
    list_parser(Platform::String^ toParse)
        : toParse_(toParse)
        , it_(toParse->Begin(), toParse->Begin() + toParse->Length(), gTokenRegex)
    {
    }

    bool try_get_next(float* value)
    {
        if (it_ == std::regex_token_iterator<const wchar_t*>())
            return false;

        *value = static_cast<float>(_wtof(it_->str().c_str()));
        ++it_;

        return true;
    }
};


std::unique_ptr<viewBox> parse_viewBox(Platform::String^ viewBoxString)
{
    float x, y, w, h;

    list_parser parser(viewBoxString);

    if (!(parser.try_get_next(&x) 
        && parser.try_get_next(&y) 
        && parser.try_get_next(&w) 
        && parser.try_get_next(&h)))
    {
        return nullptr;
    }

    return std::make_unique<viewBox>(x, y, w, h);
}


std::unique_ptr<viewBox> parse_viewBox(IXmlNode^ element)
{
    auto attribute = element->Attributes->GetNamedItemNS(SVG_NS, L"viewBox");
    if (!attribute)
        return nullptr;

    auto viewBoxString = dynamic_cast<Platform::String^>(attribute->NodeValue);
    if (!viewBoxString)
        return nullptr;

    return parse_viewBox(viewBoxString);
}


Platform::String^ get_attribute(IXmlNode^ element, Platform::String^ name)
{
    auto attribute = element->Attributes->GetNamedItem(name);
    if (!attribute)
        return nullptr;

    auto attributeString = dynamic_cast<Platform::String^>(attribute->NodeValue);
    if (!attributeString)
        return nullptr;

    return attributeString;
}


std::wregex gLengthRegex(L"(-?[0-9.]+)(em|ex|px|in|cm|mm|pt|pc|%)?");

length parse_length(Platform::String^ lengthString, length defaultLength)
{
    std::wcmatch match;

    if (!std::regex_match(lengthString->Data(), lengthString->Data() + lengthString->Length(), match, gLengthRegex))
        return defaultLength;

    float number = static_cast<float>(_wtof(match[1].str().c_str()));

    if (!match[2].matched)
        return length{ number, unit::unspecified };
   
    auto unitString = match[2].str();

    static std::pair<std::wstring, unit> unitNames[] =
    { 
        { L"em", unit::em },
        { L"ex", unit::ex },
        { L"px", unit::px },
        { L"in", unit::in },
        { L"cm", unit::cm },
        { L"mm", unit::mm },
        { L"pt", unit::pt },
        { L"pc", unit::pc },
        { L"%",  unit::percent }
    };
    
    for (auto const& unitName : unitNames)
    {
        if (unitName.first == unitString)
            return length{ number, unitName.second };
    }
    
    return defaultLength;
}


length parse_length(IXmlNode^ element, Platform::String^ name, length defaultLength)
{
    auto attributeString = get_attribute(element, name);
    if (!attributeString)
        return defaultLength;

    return parse_length(attributeString, defaultLength);
}


length parse_width_or_height(IXmlNode^ element, Platform::String^ name)
{
    return parse_length(element, name, length{ 100, unit::percent });
}


length parse_coordinate(IXmlNode^ element, Platform::String^ name)
{
    return parse_length(element, name, length{ 0, unit::unspecified });
}


paint parse_paint(IXmlNode^ element, Platform::String^ name)
{
    auto attributeString = get_attribute(element, name);

    if (attributeString == L"none")
        return paint(paint_type::none, Color{});

    // TODO: proper color parsing!

    if (attributeString == L"black")
        return paint(paint_type::color, Colors::Black);

    if (attributeString == L"green")
        return paint(paint_type::color, Colors::Green);

    if (attributeString == L"lime")
        return paint(paint_type::color, Colors::Lime);

    if (attributeString == L"yellow")
        return paint(paint_type::color, Colors::Yellow);

    if (attributeString == L"blue")
        return paint(paint_type::color, Colors::Blue);

    return paint(paint_type::color, Colors::HotPink);
}


static std::unique_ptr<element> parse_any_element(IXmlNode^ node);

template<typename SVG>
void add_children(SVG* svg, IXmlNode^ xml)
{
    for (auto const& childNode : xml->ChildNodes)
    {
        if (childNode->NodeType != NodeType::ElementNode)
            continue;

        auto childElement = parse_any_element(childNode);

        if (childElement)
            svg->add_child(std::move(childElement));
    }
}


std::unique_ptr<svg> parse_svg(IXmlNode^ svgElement)
{
    auto svgNode = std::make_unique<svg>(
        parse_viewBox(svgElement),
        parse_width_or_height(svgElement, L"width"),
        parse_width_or_height(svgElement, L"height"));

    add_children(svgNode.get(), svgElement);

    return svgNode;
}


std::unique_ptr<group> parse_g(IXmlNode^ node)
{
    auto g = std::make_unique<group>();

    add_children(g.get(), node);

    return g;
}


std::unique_ptr<circle> parse_circle(IXmlNode^ node)
{
    return std::make_unique<circle>(
        parse_paint(node, L"fill"),
        parse_paint(node, L"stroke"),
        parse_coordinate(node, L"cx"),
        parse_coordinate(node, L"cy"),
        parse_coordinate(node, L"r"));
}


std::unique_ptr<element> parse_any_element(IXmlNode^ node)
{
    auto fullName = node->NodeName;
    auto localName = node->LocalName;
    auto nsuri = node->NamespaceUri;

    auto nsuri_uri = dynamic_cast<Windows::Foundation::Uri^>(nsuri);
    auto nsuri_str = dynamic_cast<Platform::String^>(nsuri);
    

    if (dynamic_cast<Platform::String^>(node->NamespaceUri) != SVG_NS)
        return nullptr;

    auto name = dynamic_cast<Platform::String^>(node->LocalName);

    if (name == L"svg")
        return parse_svg(node);
    else if (name == L"g")
        return parse_g(node);
    else if (name == L"circle")
        return parse_circle(node);
    else
        return nullptr;
}



std::unique_ptr<svg> parse_svg(XmlDocument^ svgDocument)
{
    auto svgElement = svgDocument->SelectSingleNodeNS("svg:svg", XMLNS_SVG);
    return parse_svg(svgElement);
}

