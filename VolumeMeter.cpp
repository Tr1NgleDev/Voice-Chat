#ifndef VC_SERVER_ONLY
#include "VolumeMeter.h"

using namespace fdm;

void VolumeMeter::render(gui::Window* w)
{
	double a = glm::clamp((100 - (-value)) / 100.0, 0.0, 1.0);
	double b = glm::pow(a, 8.0);

	int x, y;
	getPos(w, &x, &y);

	QuadRenderer* qr = w->getQuadRenderer();

	qr->setQuadRendererMode(QuadRenderer::MODE_FILL);
	qr->setPos(x, y, width * a, height);
	qr->setColor(a, 1.0 - a, 0.0, 1.0);
	qr->render();
}

void VolumeMeter::offsetX(int offset)
{
	xOffset = offset;
}
void VolumeMeter::offsetY(int offset)
{
	yOffset = offset;
}
void VolumeMeter::alignX(gui::AlignmentX a)
{
	xAlign = a;
}
void VolumeMeter::alignY(gui::AlignmentY a)
{
	yAlign = a;
}

bool VolumeMeter::enabled()
{
	return false;
}

void VolumeMeter::getPos(const gui::Window* w, int* x, int* y) const
{
	int wWidth, wHeight;
	w->getSize(&wWidth, &wHeight);

	switch (xAlign)
	{
	case fdm::gui::ALIGN_LEFT:
		*x = xOffset;
		break;
	case fdm::gui::ALIGN_RIGHT:
		*x = wWidth + xOffset - width;
		break;
	case fdm::gui::ALIGN_CENTER_X:
		*x = xOffset + wWidth / 2 - width / 2;
		break;
	}

	switch (yAlign)
	{
	case fdm::gui::ALIGN_TOP:
		*y = yOffset;
		break;
	case fdm::gui::ALIGN_BOTTOM:
		*y = wHeight + yOffset - height;
		break;
	case fdm::gui::ALIGN_CENTER_Y:
		*y = yOffset + wHeight / 2 - height / 2;
		break;
	}
}
void VolumeMeter::getSize(const gui::Window* w, int* width, int* height) const
{
	*width = this->width;
	*height = this->height;
}

void VolumeMeter::update(double dt)
{
	if (value >= -100.0)
		value -= (100.0 / decreaseRate) * dt;
}

void VolumeMeter::setValue(double dB)
{
	value = glm::max(glm::clamp(dB, -100.0, 0.0), value);
}
#endif
