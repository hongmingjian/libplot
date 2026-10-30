#ifndef PLOT_HPP
#define PLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <regex>
#include <cassert>

#include "libplot.h"

class Plot;
class Curve;

struct Tick {
    float interval = 0.0f; // 0 for auto ticks
    bool visible = true;
    std::string color = "#000000";
};

struct Grid {
    bool visible = true;
    std::string color = "#C0C0C0";
    std::string style = ":";
};

struct Axis {
	Plot *plot = nullptr;

    int scale = SCALE_LINEAR;
    float min = 0.0f;
    float max = 100.0f;
	float ratioMargin = 0.08f;

	float width = 1.0f;
    bool visible = true;
    bool panEnabled = true;
    bool zoomEnabled = true;
    std::string label = "";
    std::string color = "#000000";
    std::string style = "-";
	std::string unit = "";

	bool autorange = true;
	bool autoscroll = false;

    Tick tick;
    Grid grid;

	static constexpr int END_MIN = 0x1;
	static constexpr int END_MAX = 0x2;
	void applyMargin(int ends = END_MIN|END_MAX);
    void pan(float d);
	void zoom(float center, float factor);
	std::vector<float> calculateTicks() const;
};

class Plot {
private:
	friend class Axis;
	friend class Curve;

	int coord = COORD_CARTESIAN;

	float plotWidth = 400.0f; //pixels
	float plotHeight = 300.0f; //pixels
	float fontWidth = 12.0f; //pixels
	float fontHeight = 15.0f; //pixels

	std::map<std::string, float> paddings = {{"L", 60}, {"T", 40}, {"R", 60}, {"B", 40}}; //pixels

	std::string title = "";
    std::string backgroundColor = ""; // empty for transparent

	std::vector<Curve *> curves;
	Curve *addCurve(Curve *curve);
	Curve *removeCurve(Curve *curve);

    void applyMargin();
	void autoRange(bool force = true);
    float getClientWidth() const { return plotWidth - paddings.at("L") - paddings.at("R"); }
    float getClientHeight() const { return plotHeight - paddings.at("T") - paddings.at("B"); }
public:
	Axis axisX;   // as theta in Polar
	Axis axisY;   // primary (left) Y, as r in Polar
	Axis axisY2;  // secondary (right) Y
	int activeY = AXIS_PRIMARY;

    Plot(int coord = COORD_CARTESIAN);
    ~Plot();

	int getCoord() const { return coord; }

    float toPixelX(float mathX) const;
    float toPixelY(float mathY, int which = AXIS_PRIMARY) const;
    float toMathX(float pixelX) const;
    float toMathY(float pixelY, int which = AXIS_PRIMARY) const;
    std::pair<float, float> toPolarPixel(float theta, float r) const;
    std::pair<float, float> toPolarMath(float pixelX, float pixelY) const;
	void pix2math(int pixelX, int pixelY, float *x, float *y, float *y2) const;
    void pan(int pixelXDelta, int pixelYDelta);
    void zoom(int pixelX, int pixelY, float factor);
    int set_option(const std::string& option, const std::string& value);
	int render(struct plt_render_cmd *out, size_t nout) const;
};

struct Curve {
	Plot *plot = nullptr;
	int axisY = AXIS_PRIMARY;

	bool visible = true;
    float width = 2.0f; // pixels
    std::string label = "";
    std::string color = "#000000";
    std::string style = "-";

	std::string markerShape = "";
	float markerSize = 6.0f; // pixels
	std::string markerColor = "#000000";

    Curve(Plot *plot) {
		this->plot = plot;
		axisY = plot->activeY;

		plot->addCurve(this);
	}
    ~Curve() {
		plot->removeCurve(this);
	}
	const std::vector<float> &getXData() const { return xData; }
	const std::vector<float> &getYData() const { return yData; }
	std::pair<float, float> getXMinMax() const { return {xmin, xmax}; }
	std::pair<float, float> getYMinMax() const { return {ymin, ymax}; }

	void updateData(const std::vector<float> &x, const std::vector<float> &y);
	void appendData(const std::vector<float> &x, const std::vector<float> &y);
    int set_option(const std::string& option, const std::string& value);

private:
    std::vector<float> xData;
    std::vector<float> yData;

	// cache
	float xmin, xmax, ymin, ymax;
};

#endif // PLOT_HPP
