#pragma once

#include <4dm.h>

class ImgButton : public fdm::gui::Button
{
public:
	fdm::TexRenderer tex;

	bool visible = true;
	int edgeW = 1;
	int edgeH = 1;

	ImgButton() {}
	ImgButton(const fdm::Tex2D* texture, int width = 0, int height = 0, int edgeW = 1, int edgeH = 1);

	ImgButton(ImgButton&& other) noexcept;
	ImgButton& operator=(ImgButton&& other) noexcept;

	void render(fdm::gui::Window* w) override;

	void setTexture(const fdm::Tex2D* texture);
};
