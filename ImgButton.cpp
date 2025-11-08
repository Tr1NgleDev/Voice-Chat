#ifndef VC_SERVER_ONLY
#include "ImgButton.h"

using namespace fdm;
using namespace fdm::gui;

ImgButton::ImgButton(const Tex2D* texture, int width, int height, int edgeW, int edgeH)
	: edgeW(edgeW), edgeH(edgeH)
{
	tex.shader = ShaderManager::get("tex2DShader");
	tex.texture = texture;
	tex.init();
	setTexture(texture);
	tex.setColor(1, 1, 1, 1);

	this->width = width;
	this->height = height;
}

ImgButton::ImgButton(ImgButton&& other) noexcept
	: Button(other), tex(std::move(other.tex)), edgeW(std::move(other.edgeW)), edgeH(std::move(other.edgeH)) { }

ImgButton& ImgButton::operator=(ImgButton&& other) noexcept
{
	if (this != &other)
	{
		Button::operator=(std::move(other));

		tex = std::move(other.tex);
		edgeW = std::move(other.edgeW);
		edgeH = std::move(other.edgeH);
	}
	return *this;
}

void ImgButton::render(Window* w)
{
	if (!visible) return;

	Button::render(w);

	int x, y;
	getPos(w, &x, &y);

	tex.setPos(x + edgeW, y + edgeH, width - edgeW * 2, height - edgeW * 2);
	tex.render();
}

void ImgButton::setTexture(const Tex2D* texture)
{
	tex.texture = texture;
	tex.setClip(0, 0, tex.texture->width, tex.texture->height);
}
#endif
