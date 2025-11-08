#pragma once

#include <4dm.h>

class VolumeMeter : public fdm::gui::Element
{
private:
	double value = -100;

public:
	int xOffset = 0;
	int yOffset = 0;
	fdm::gui::AlignmentX xAlign = fdm::gui::ALIGN_LEFT;
	fdm::gui::AlignmentY yAlign = fdm::gui::ALIGN_TOP;
	int width = 100;
	int height = 50;
	double decreaseRate = 0.75;

	void render(fdm::gui::Window* w) override;

	void offsetX(int offset) override;
	void offsetY(int offset) override;
	void alignX(fdm::gui::AlignmentX a) override;
	void alignY(fdm::gui::AlignmentY a) override;

	bool enabled() override;

	void getPos(const fdm::gui::Window* w, int* x, int* y) const override;
	void getSize(const fdm::gui::Window* w, int* width, int* height) const override;

	void update(double dt);
	void setValue(double dB);
};
